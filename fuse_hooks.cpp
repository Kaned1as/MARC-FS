#include "fuse_hooks.h"

#include <cstring>

using namespace std;

int getattr_callback(const char *path, struct stat *stbuf)
{
    // retrieve path to containing dir
    fuse_context *ctx = fuse_get_context();
    string pathStr(path); // e.g. /home/1517.svg
    if (pathStr == "/") {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    auto slashPos = pathStr.find_last_of('/'); // that would be 5
    if (slashPos == string::npos)
        return -EIO;

    string dirname = pathStr.substr(0, slashPos); // /home
    string filename = pathStr.substr(slashPos + 1);
    auto api = fsMetadata.apiPool.acquire();
    auto contents = api->ls(dirname); // actual API call - ls the directory that contains this file
    auto file = find_if(contents.cbegin(), contents.cend(), [&](const CloudFile &file) {
        return file.getName() == filename; // now find file in returned results
    });

    if (file != contents.cend()) {
        stbuf->st_uid = ctx->uid; // file is always ours, as long as we're authenticated
        stbuf->st_gid = ctx->gid;
        stbuf->st_mtim.tv_sec = static_cast<int64_t>((*file).getMtime());
        if ((*file).getType() == CloudFile::Directory) { // is it a directory?
            stbuf->st_mode = S_IFDIR | 0700;
            stbuf->st_nlink = 2;
        } else if ((*file).getType() == CloudFile::File) { // or a file?
            stbuf->st_mode = S_IFREG | 0600; // cloud files are readable, but don't launch them
            stbuf->st_nlink = 1;
            stbuf->st_size = static_cast<off_t>((*file).getSize()); // offset is 32 bit on x86 platforms
        }
        return 0;
    }

    return -ENOENT;
}

int statfs_callback(const char *path, struct statfs *stat)
{

}

int utime_callback(const char *path, utimbuf *utime)
{

}

int open_callback(const char *path, int mode)
{
    auto api = fsMetadata.apiPool.acquire();
    fsMetadata.cached[path] = api->download(path);
    return 0;
}

int release_callback(const char *path, int mode)
{
    if (!fsMetadata.dirty[path])
        return 0;

    auto it = fsMetadata.cached.find(path);
    if (it == fsMetadata.cached.end())
        return -EBADR; // Should be cached after open() call

    auto api = fsMetadata.apiPool.acquire();
    api->upload(it->second, path);

    fsMetadata.dirty[path] = false;
    fsMetadata.cached.erase(it);
    return 0;
}

int readdir_callback(const char *path, fuse_dirh_t dirhandle, fuse_dirfil_t_compat filler)
{
    filler(dirhandle, ".", 0);
    filler(dirhandle, "..", 0);

    string pathStr(path); // e.g. /directory or /
    auto api = fsMetadata.apiPool.acquire();
    auto contents = api->ls(pathStr);
    for (CloudFile &cf : contents) {
        filler(dirhandle, cf.getName().data(), 0);
    }

    return 0;
}

int read_callback(const char *path, char *buf, size_t size, off_t offset)
{
    auto offsetBytes = static_cast<uint64_t>(offset);
    auto it = fsMetadata.cached.find(path);
    if (it == fsMetadata.cached.end())
        return -EBADR; // Should be cached after open() call

    auto &vec = it->second;
    auto len = vec.size();

    if (offsetBytes > len)
        return 0; // requested bytes above the size

    if (offsetBytes + size > len) {
        // requested size is more than we have
        memcpy(buf, &vec[0] + offsetBytes, len - offsetBytes);
        return static_cast<int>(len - offsetBytes);
    }

    memcpy(buf, &vec[0] + offsetBytes, size);
    return static_cast<int>(size);
}

int write_callback(const char *path, const char *buf, size_t size, off_t offset)
{
    auto offsetBytes = static_cast<uint64_t>(offset);
    auto it = fsMetadata.cached.find(path);
    if (it == fsMetadata.cached.end())
        return -EBADR; // Should be cached after open() call

    auto &vec = it->second;
    if (offsetBytes + size > vec.size()) {
        vec.resize(offsetBytes + size);
    }

    memcpy(&vec[offsetBytes], &buf[0], size);
    fsMetadata.dirty[path] = true;
    return static_cast<int>(size);
}

int flush_callback(const char *path)
{
    return 0;
}

int mkdir_callback(const char *path, mode_t mode)
{
    auto api = fsMetadata.apiPool.acquire();
    api->mkdir(path);
    return 0;
}

int rmdir_callback(const char *path)
{
    auto api = fsMetadata.apiPool.acquire();
    auto contents = api->ls(path);
    if (!contents.empty())
        return -ENOTEMPTY;


    api->remove(path);
    return 0;
}

int unlink_callback(const char *path)
{
    auto api = fsMetadata.apiPool.acquire();
    api->remove(path);
    return 0;
}

int rename_callback(const char *oldPath, const char *newPath)
{

}

int truncate_callback(const char *path, off_t size)
{
    auto it = fsMetadata.cached.find(path);
    if (it == fsMetadata.cached.end())
        return -EBADR; // Should be cached after open() call

    auto &vec = it->second;
    vec.resize(static_cast<uint64_t>(size));
    return 0;
}

int mknod_callback(const char *path, mode_t mode, dev_t dev)
{
    auto api = fsMetadata.apiPool.acquire();
    api->upload(vector<char>(), path);
    return 0;
}
