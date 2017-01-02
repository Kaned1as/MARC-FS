#ifndef FUSE_HOOKS_H
#define FUSE_HOOKS_H

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include "mru_metadata.h"

extern MruData fsMetadata;

void * initCallback(struct fuse_conn_info *conn);

int getattrCallback(const char *path, struct stat *stbuf);
int statfsCallback(const char *path, struct statvfs *stat);
int utimeCallback(const char *path, struct utimbuf *utime);

int openCallback(const char *path, struct fuse_file_info *fi);
int releaseCallback(const char *path, struct fuse_file_info *fi);

/**
 * @note mknodCallback - invoked when file is absent when writing
 */
int mknodCallback(const char *path, mode_t mode, dev_t dev);
int readdirCallback(const char *path, void *dirhandle, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
int readCallback(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
/**
 * @note writeCallbackAsync - doesn't work due to cloud API limitation, see @fn write_callback
 */
int writeCallbackAsync(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int writeCallback(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int truncateCallback(const char *path, off_t size);
int flushCallback(const char *path, fuse_file_info *fi);

int mkdirCallback(const char *path, mode_t mode);
int rmdirCallback(const char *path);

int unlinkCallback(const char *path);
int renameCallback(const char *oldPath, const char *newPath);

#endif // FUSE_HOOKS_H
