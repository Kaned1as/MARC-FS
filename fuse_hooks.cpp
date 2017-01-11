#include "fuse_hooks.h"

#include <malloc.h>
#include <cstring>
#include <thread>

#define MARCFS_MEMTRIM          malloc_trim(64 * 1024 * 1024); // force OS to reclaim memory above 64KiB from us

#define API_CALL_TRY_BEGIN  \
    try { \
        auto client = fsMetadata.clientPool.acquire();

#define API_CALL_TRY_FINISH \
    } catch (MailApiException &exc) { \
        if (exc.getResponseCode() == 500) \
            return -EAGAIN; \
        return -EIO;\
    }

using namespace std;

static void fillStats(struct stat *stbuf, const CloudFile &cf) {
    auto ctx = fuse_get_context();

    stbuf->st_uid = ctx->uid; // file is always ours, as long as we're authenticated
    stbuf->st_gid = ctx->gid;
    stbuf->st_mtim.tv_sec = static_cast<long>(cf.getMtime());
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

    auto node = fsMetadata.getFile(path);
    if (node) {
        unique_lock<mutex> unlocker(node->getMutex(), adopt_lock); // unlock file mutex in case of exception
        if (node->hasStat()) {
            struct stat stats = node->getStat();
            memcpy(stbuf, &stats, sizeof(struct stat));
            return 0;
        }
    }

    auto slashPos = pathStr.find_last_of('/'); // that would be 5
    if (slashPos == string::npos)
        return -EIO;

    // find requested file on cloud
    string dirname = pathStr.substr(0, slashPos + 1); // /home
    string filename = pathStr.substr(slashPos + 1);

    // API call required
    API_CALL_TRY_BEGIN
    auto contents = client->ls(dirname); // actual API call - ls the directory that contains this file

    auto file = find_if(contents.cbegin(), contents.cend(), [&](const CloudFile &file) {
        return file.getName() == filename; // now find file in returned results
    });

    // file found - fill statbuf
    if (file != contents.cend()) {
        fillStats(stbuf, *file);
        fsMetadata.putCacheStat(path, stbuf);
        return 0;
    }
    API_CALL_TRY_FINISH


    // file not found
    return -ENOENT;
}

int statfsCallback(const char */*path*/, struct statvfs *stat)
{
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

    API_CALL_TRY_BEGIN
    auto info = client->df();
    stat->f_fsid = {}; // ignored
    stat->f_bsize = 4096; // a guess!
    stat->f_blocks = info.totalMiB * 256; // * 1024 * 1024 / f_bsize
    stat->f_bfree = stat->f_blocks - info.usedMiB * 256;
    stat->f_bavail = stat->f_bfree;
    stat->f_namemax = 256;
    return 0;
    API_CALL_TRY_FINISH

}

int utimeCallback(const char */*path*/, utimbuf */*utime*/)
{
    // stub
    return 0;
}


int createCallback(const char *path, mode_t mode, fuse_file_info *fi)
{
    auto res = mknodCallback(path, mode, 0);
    if (res)
        return res;

    return openCallback(path, fi);
}

int openCallback(const char *path, struct fuse_file_info *fi)
{
    auto api = fsMetadata.clientPool.acquire();
    auto file = fsMetadata.getOrCreateFile(path);
    unique_lock<mutex> unlocker(file->getMutex(), adopt_lock); // unlock file mutex in case of exception

    API_CALL_TRY_BEGIN
    file->setCachedContent(client->download(path));
    API_CALL_TRY_FINISH

    return 0;
}

int readdirCallback(const char *path, void *dirhandle, fuse_fill_dir_t filler, off_t /*offset*/, struct fuse_file_info */*fi*/)
{
    filler(dirhandle, ".", nullptr, 0);
    filler(dirhandle, "..", nullptr, 0);

    string pathStr(path); // e.g. /directory or /

    API_CALL_TRY_BEGIN
    auto contents = client->ls(pathStr);
    for (const CloudFile &cf : contents) {
        struct stat stbuf = {};
        fillStats(&stbuf, cf);
        if (cf.getType() == CloudFile::File) {
            fsMetadata.putCacheStat(pathStr + "/" + cf.getName(), &stbuf);
        }
        filler(dirhandle, cf.getName().data(), &stbuf, 0);
    }
    API_CALL_TRY_FINISH

    return 0;
}


//int readCallbackAsync(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info */*fi*/)
//{
//    auto offsetBytes = static_cast<uint64_t>(offset);
//    uint64_t readNow = 0;
//    auto callRead = [&](auto &pipe) {
//        while (!pipe.exhausted()) {
//            // get next chunk, possibly blocking
//            readNow += pipe.pop(buf + readNow, size - readNow); // actual transfer of bytes to the buffer
//            if (readNow == size) // reached limit
//                break;
//        }
//    };
    
//    // we're reading this file, try to pipe it from the cloud
//    if (offsetBytes == 0) { // that's a start
//        // set up async API transfer that will fill our queue
//        // api object will return to the pool once download is complete
//        auto &pipe = fsMetadata.createTransfer(path);
//        auto handle = [&]() {
//            auto api = fsMetadata.clientPool.acquire();
//            auto file = fsMetadata.getOrCreateFile(path);
//            unique_lock<mutex> unlocker(file->getMutex(), adopt_lock); // lock file till read is complete
//            api->downloadAsync(path, pipe);
//        };
//        thread(handle).detach();

//        callRead(pipe);
//    } else {
//        auto &pipe = fsMetadata.getTransfer(path);
//        if (offsetBytes == pipe.getTransferred()) { // that's continuation of previous call
//        // transfer must already been set, continue
//            callRead(pipe);
//        } else {
//            // attempt to read a file not sequentially, abort
//            return -EBADE;
//        }
//    }

//    return static_cast<int>(readNow);
//}

// not working, see header file
//int writeCallbackAsync(const char *path, const char *buf, size_t size, off_t offset,struct fuse_file_info */*fi*/)
//{
//    auto offsetBytes = static_cast<uint64_t>(offset);
//    auto &file = fsMetadata.obtainFile(path);
//    unique_lock<mutex> unlocker(node->getMutex(), adopt_lock); // unlock file mutex in case of exception

//    auto writtenAlready = file->getTransferred();
//    vector<char> vec(buf, buf + size);
//    // we're writing this file, try to send it chunked to the cloud
//    if (offsetBytes == 0) { // that's a start
//        auto api = fsMetadata.clientPool.acquire();
//        // set up async API transfer that will fill our queue
//        auto handle = bind(&MarcRestClient::uploadAsync, api.get(), path, ref(file->getTransfer()));
//        thread(handle).detach();

//        file->getTransfer().push(vec); // actual transfer of bytes to the queue
//        file->setTransferred(writtenAlready + size);
//    } else if (offsetBytes == writtenAlready) { // that's continuation of previous call
//        // transfer must already been set, continue
//        file->getTransfer().push(vec);
//    } else {
//        // attempt to write a file not sequentially, abort
//        return -EBADE;
//    }

//    file->setDirty(true);
//    file->setTransferred(writtenAlready + size);
//    return static_cast<int>(size);
//}

int readCallback(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info */*fi*/)
{
    auto offsetBytes = static_cast<uint64_t>(offset);
    auto file = fsMetadata.getFile(path);
    unique_lock<mutex> unlocker(file->getMutex(), adopt_lock); // unlock file mutex in case of exception
    auto &vec = file->getCachedContent();
    auto len = vec.size();
    if (offsetBytes > len)
        return 0; // requested bytes above the size

    if (offsetBytes + size > len) {
        // requested size is more than we have
        copy_n(&vec.front() + offsetBytes, len - offsetBytes, buf);
        return static_cast<int>(len - offsetBytes);
    }

    // normal operation
    copy_n(&vec.front() + offsetBytes, size, buf);
    return static_cast<int>(size);
}

int writeCallback(const char *path, const char *buf, size_t size, off_t offset, fuse_file_info */*fi*/)
{
    auto offsetBytes = static_cast<uint64_t>(offset);
    auto file = fsMetadata.getOrCreateFile(path);
    unique_lock<mutex> unlocker(file->getMutex(), adopt_lock); // unlock file mutex in case of exception

    auto &vec = file->getCachedContent();
    if (offsetBytes + size > vec.size()) {
        vec.resize(offsetBytes + size);
    }

    copy_n(buf, size, &vec.front() + offsetBytes);
    file->setDirty(true);
    return static_cast<int>(size);
}


int flushCallback(const char *path, struct fuse_file_info */*fi*/)
{
    auto file = fsMetadata.getOrCreateFile(path);
    unique_lock<mutex> unlocker(file->getMutex(), adopt_lock); // unlock file mutex in case of exception

    if (!file->isDirty())
        return 0;

    API_CALL_TRY_BEGIN
    client->upload(path, file->getCachedContent());

    // update size of file
    struct stat stats = file->getStat();
    stats.st_size = static_cast<long>(file->getCachedContent().size());
    file->setStat(stats);

    file->setDirty(false);
    API_CALL_TRY_FINISH

    return 0;
}

int releaseCallback(const char *path, struct fuse_file_info */*fi*/)
{
    auto file = fsMetadata.getOrCreateFile(path);
    unique_lock<mutex> unlocker(file->getMutex(), adopt_lock); // unlock file mutex in case of exception
    file->getCachedContent().clear(); // forget contents of a node
    file->getCachedContent().shrink_to_fit();
    MARCFS_MEMTRIM

    return 0;
}

int mkdirCallback(const char *path, mode_t /*mode*/)
{
    API_CALL_TRY_BEGIN
    client->mkdir(path);
    API_CALL_TRY_FINISH
    return 0;
}

int rmdirCallback(const char *path)
{
    API_CALL_TRY_BEGIN
    auto contents = client->ls(path);
    if (!contents.empty())
        return -ENOTEMPTY;

    client->remove(path);
    API_CALL_TRY_FINISH

    return 0;
}

int unlinkCallback(const char *path)
{
    API_CALL_TRY_BEGIN
    client->remove(path);
    fsMetadata.purgeCache(path);
    API_CALL_TRY_FINISH

    return 0;
}

int renameCallback(const char *oldPath, const char *newPath)
{
    API_CALL_TRY_BEGIN
    client->rename(oldPath, newPath);
    fsMetadata.purgeCache(oldPath);
    fsMetadata.purgeCache(newPath);
    API_CALL_TRY_FINISH

    return 0;
}

int truncateCallback(const char *path, off_t size)
{
    auto file = fsMetadata.getOrCreateFile(path);
    unique_lock<mutex> unlocker(file->getMutex(), adopt_lock); // unlock file mutex in case of exception

    auto &vec = file->getCachedContent();
    vec.resize(static_cast<uint64_t>(size));
    return 0;
}

int mknodCallback(const char *path, mode_t /*mode*/, dev_t /*dev*/)
{
    API_CALL_TRY_BEGIN
    vector<char> tmp; // lvalue
    client->upload(path, tmp);
    API_CALL_TRY_FINISH

    return 0;
}

int chmodCallback(const char */*path*/, mode_t /*mode*/)
{
    // stub, no access rights for your own cloud
    return 0;
}
