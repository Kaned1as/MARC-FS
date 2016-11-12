#include "fuse_hooks.h"

int getattr_callback(const char *path, struct stat *stbuf)
{

}

int statfs_callback(const char *path, struct statfs *stat)
{

}

int utime_callback(const char *path, utimbuf *utime)
{

}

int open_callback(const char *path, int mode)
{

}

int release_callback(const char *, int mode)
{

}

int readdir_callback(const char *path, fuse_dirh_t dirhandle, fuse_dirfil_t_compat filler)
{

}

int read_callback(const char *path, char *buf, size_t size, off_t offset)
{

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
