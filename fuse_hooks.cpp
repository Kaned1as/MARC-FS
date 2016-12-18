#include "fuse_hooks.h"
#include "fscache.h"

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
    auto contents = api.ls(dirname); // actual API call - ls the directory that contains this file
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
    FsCache<string, vector<int8_t>>::instance();
    fuse_context *ctx = fuse_get_context();
    string pathStr(path); // e.g. /home/1517.svg
    ctx->private_data = new vector<int8_t>(api.download(pathStr));
    return 0; // TODO: caching
}

int release_callback(const char *, int mode)
{

}

int readdir_callback(const char *path, fuse_dirh_t dirhandle, fuse_dirfil_t_compat filler)
{
    filler(dirhandle, ".", 0);
    filler(dirhandle, "..", 0);

    string pathStr(path); // e.g. /directory or /
    auto contents = api.ls(pathStr);
    for (CloudFile &cf : contents) {
        filler(dirhandle, cf.getName().data(), 0);
    }

    return 0;
}

int read_callback(const char *path, char *buf, size_t size, off_t offset)
{
    fuse_context *ctx = fuse_get_context();
    auto data = reinterpret_cast<vector<int8_t> *>(ctx->private_data);
    auto start = data->data()[offset];

}

int write_callback(const char *path, const char *buf, size_t size, off_t offset)
{

}

int flush_callback(const char *path)
{

}

int mkdir_callback(const char *path, mode_t mode)
{

}

int rmdir_callback(const char *path)
{

}

int unlink_callback(const char *path)
{

}

int rename_callback(const char *oldPath, const char *newPath)
{

}
