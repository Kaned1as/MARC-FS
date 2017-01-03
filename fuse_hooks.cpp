#include "fuse_hooks.h"

#include <malloc.h>
#include <cstring>
#include <future>

#define MARCFS_MEMTRIM          malloc_trim(64 * 1024 * 1024); // force OS to reclaim memory above 64KiB from us

using namespace std;

static void fillStats(struct stat *stbuf, const CloudFile &cf) {
    auto ctx = fuse_get_context();

    stbuf->st_uid = ctx->uid; // file is always ours, as long as we're authenticated
    stbuf->st_gid = ctx->gid;
    if (cf.getType() == CloudFile::Directory) { // is it a directory?
        stbuf->st_mode = S_IFDIR | 0700;
        stbuf->st_nlink = 2;
    } else if (cf.getType() == CloudFile::File) { // or a file?
        stbuf->st_mode = S_IFREG | 0600; // cloud files are readable, but don't launch them
        stbuf->st_nlink = 1;
        stbuf->st_size = static_cast<off_t>(cf.getSize()); // offset is 32 bit on x86 platforms
    }
};


void *initCallback(fuse_conn_info *conn)
{
    conn->want |= FUSE_CAP_BIG_WRITES; // writes more than 4096

    // confusing, right?
    // one would think we support async reads because we invoke downloadAsync from API
    // but here's the thing - these download-related reads come *sequentially*,
    // with each next offset being equal to last + total.
    // FUSE_CAP_ASYNC_READ value means that multiple reads with different offsets
    // will come in one time. We surely can't support this.
    conn->want &= static_cast<unsigned>(~FUSE_CAP_ASYNC_READ);
    conn->async_read = 0;
    return nullptr;
}

int getattrCallback(const char *path, struct stat *stbuf)
{
    // retrieve path to containing dir
    string pathStr(path); // e.g. /home/1517.svg
    if (pathStr == "/") {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    auto slashPos = pathStr.find_last_of('/'); // that would be 5
    if (slashPos == string::npos)
        return -EIO;

    // find requested file on cloud
    string dirname = pathStr.substr(0, slashPos); // /home
    string filename = pathStr.substr(slashPos + 1);
    auto api = fsMetadata.clientPool.acquire();
    auto contents = api->ls(dirname); // actual API call - ls the directory that contains this file
    auto file = find_if(contents.cbegin(), contents.cend(), [&](const CloudFile &file) {
        return file.getName() == filename; // now find file in returned results
    });

    // file found - fill statbuf
    if (file != contents.cend()) {
        fillStats(stbuf, *file);
        return 0;
    }

    // file not found
    return -ENOENT;
}

int statfsCallback(const char */*path*/, struct statvfs *stat)
{
    auto api = fsMetadata.clientPool.acquire();
    auto info = api->df();

    /*
        uint32 f_bsize; // optimal transfer block size
        uint32 f_blocks; // total block count in fs
        uint32 f_bfree; // free block count in fs
        uint32 f_bavail; // free block count available for non-root
        uint32 f_files; // total node count in filesystem
        uint32 f_ffree; // free node count in filesystem
        struct { int sid[2]; } f_fsid; // ids - major, minor of filesystem
        uint32 f_namelen; // maximum file naming length
        uint32 f_spare[6]; // reserved
    */

    stat->f_fsid = {}; // ignored
    stat->f_bsize = 4096; // a guess!
    stat->f_blocks = info.totalMiB * 256; // * 1024 * 1024 / f_bsize
    stat->f_bfree = stat->f_blocks * (100 - info.usedPercent) / 100;
    stat->f_bavail = stat->f_bfree;
    stat->f_namemax = 256;
    return 0;
}

int utimeCallback(const char */*path*/, utimbuf */*utime*/)
{
    // stub
    return 0;
}

int openCallback(const char *path, struct fuse_file_info *fi)
{
    auto api = fsMetadata.clientPool.acquire();
    fsMetadata.cached[path] = MruNode(); // create it for later use

    if (fi->flags & (O_WRONLY | O_RDWR)) {
        // file is supposedly opened for writing, we should cache it
        // due to cloud API limitation
        fsMetadata.cached[path].setCachedContent(api->download(path));
    }

    return 0;
}

int releaseCallback(const char *path, struct fuse_file_info */*fi*/)
{
    auto it = fsMetadata.cached.find(path);
    if (it == fsMetadata.cached.end())
        return -EBADR; // Should be cached after open() call

    fsMetadata.cached.erase(it); // memory cleanup
    MARCFS_MEMTRIM

    return 0;
}

int readdirCallback(const char *path, void *dirhandle, fuse_fill_dir_t filler, off_t /*offset*/, struct fuse_file_info */*fi*/)
{
    filler(dirhandle, ".", nullptr, 0);
    filler(dirhandle, "..", nullptr, 0);

    string pathStr(path); // e.g. /directory or /
    auto api = fsMetadata.clientPool.acquire();
    auto contents = api->ls(pathStr);
    for (const CloudFile &cf : contents) {
        struct stat stbuf = {};
        fillStats(&stbuf, cf);
        filler(dirhandle, cf.getName().data(), &stbuf, 0);
    }

    return 0;
}

int readCallbackAsync(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info */*fi*/)
{
    auto offsetBytes = static_cast<uint64_t>(offset);
    auto it = fsMetadata.cached.find(path);
    if (it == fsMetadata.cached.end())
        return -EBADR; // Should be cached after open() call

    auto &node = it->second;
    uint64_t readAlready = node.getTransferred(); // chunks that were already read
    uint64_t readNow = 0;
    auto callRead = [&]() {
        while (!node.getTransfer().exhausted()) {
            // get next chunk, possibly blocking
            readNow += node.getTransfer().pop(buf + readNow, size - readNow); // actual transfer of bytes to the buffer
            if (readNow == size) // reached limit
                break;
        }
    };
    
    // we're reading this file, try to pipe it from the cloud
    if (offsetBytes == 0) { // that's a start
        auto api = fsMetadata.clientPool.acquire();
        // set up async API transfer that will fill our queue
        // api object will return to the pool once download is complete
        auto handle = bind(&MarcRestClient::downloadAsync, api, path, ref(node.getTransfer()));
        thread(handle).detach();

        callRead();
    } else if (offsetBytes == readAlready) { // that's continuation of previous call
        // transfer must already been set, continue
        callRead();
    } else {
        // attempt to read a file not sequentially, abort
        return -EBADE;
    }

    node.setTransferred(readAlready + readNow);
    return static_cast<int>(readNow);
}

int writeCallbackAsync(const char *path, const char *buf, size_t size, off_t offset,struct fuse_file_info */*fi*/)
{
    auto offsetBytes = static_cast<uint64_t>(offset);
    auto it = fsMetadata.cached.find(path);
    if (it == fsMetadata.cached.end())
        return -EBADR; // Should be cached after open() call

    auto &node = it->second;
    auto writtenAlready = node.getTransferred();
    vector<char> vec(buf, buf + size);
    // we're writing this file, try to send it chunked to the cloud
    if (offsetBytes == 0) { // that's a start
        auto api = fsMetadata.clientPool.acquire();
        // set up async API transfer that will fill our queue
        auto handle = bind(&MarcRestClient::uploadAsync, api.get(), path, ref(node.getTransfer())); // TODO: returns API to the pool while download is still in progress!
        thread(handle).detach();

        node.getTransfer().push(vec); // actual transfer of bytes to the queue
        node.setTransferred(writtenAlready + size);
    } else if (offsetBytes == writtenAlready) { // that's continuation of previous call
        // transfer must already been set, continue
        node.getTransfer().push(vec);
    } else {
        // attempt to write a file not sequentially, abort
        return -EBADE;
    }

    node.setDirty(true);
    node.setTransferred(writtenAlready + size);
    return static_cast<int>(size);
}

int writeCallback(const char *path, const char *buf, size_t size, off_t offset, fuse_file_info */*fi*/)
{
    auto offsetBytes = static_cast<uint64_t>(offset);
    auto it = fsMetadata.cached.find(path);
    if (it == fsMetadata.cached.end())
        return -EBADR; // Should be cached after open() call

    auto &vec = it->second.getCachedContent();
    if (offsetBytes + size > vec.size()) {
        vec.resize(offsetBytes + size);
    }

    memcpy(&vec[offsetBytes], buf, size);
    it->second.setDirty(true);
    return static_cast<int>(size);
}


int flushCallback(const char *path, struct fuse_file_info */*fi*/)
{
    auto it = fsMetadata.cached.find(path);
    if (it == fsMetadata.cached.end())
        return -EBADR; // Should be cached after open() call

    auto &node = it->second;
    if (!node.isDirty())
        return 0;

    // node.getTransfer().markEnd(); // for async operations - not working, incomplete
    // node.transferThread.join();
    // node.setDirty(false);
    auto api = fsMetadata.clientPool.acquire(); // for sync operations
    api->upload(path, node.getCachedContent());
    return 0;
}

int mkdirCallback(const char *path, mode_t /*mode*/)
{
    auto api = fsMetadata.clientPool.acquire();
    api->mkdir(path);
    return 0;
}

int rmdirCallback(const char *path)
{
    auto api = fsMetadata.clientPool.acquire();
    auto contents = api->ls(path);
    if (!contents.empty())
        return -ENOTEMPTY;


    api->remove(path);
    return 0;
}

int unlinkCallback(const char *path)
{
    auto api = fsMetadata.clientPool.acquire();
    api->remove(path);
    return 0;
}

int renameCallback(const char *oldPath, const char *newPath)
{
    auto api = fsMetadata.clientPool.acquire();
    api->rename(oldPath, newPath);
    return 0;
}

int truncateCallback(const char *path, off_t size)
{
    auto it = fsMetadata.cached.find(path);
    if (it == fsMetadata.cached.end())
        return -EBADR; // Should be cached after open() call

    auto &vec = it->second.getCachedContent();
    vec.resize(static_cast<uint64_t>(size));
    return 0;
}

int mknodCallback(const char *path, mode_t /*mode*/, dev_t /*dev*/)
{
    auto api = fsMetadata.clientPool.acquire();
    vector<char> tmp; // lvalue
    api->upload(path, tmp);
    return 0;
}
