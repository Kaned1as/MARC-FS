#ifndef FUSE_HOOKS_H
#define FUSE_HOOKS_H

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include "mru_metadata.h"

extern MruData fsMetadata;

void * init_callback(struct fuse_conn_info *conn);

int getattr_callback(const char *path, struct stat *stbuf);
int statfs_callback(const char *path, struct statvfs *stat);
int utime_callback(const char *path, struct utimbuf *utime);

int open_callback(const char *path, struct fuse_file_info *fi);
int release_callback(const char *path, struct fuse_file_info *fi);

int mknod_callback(const char *path, mode_t mode, dev_t dev);
int readdir_callback(const char *path, void *dirhandle, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
int read_callback(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
/**
 * @brief write_callback_async - doesn't work due to cloud API limitation, see @fn write_callback
 */
int write_callback_async(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int write_callback(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int truncate_callback(const char *path, off_t size);
int flush_callback(const char *path, fuse_file_info *fi);

int mkdir_callback(const char *path, mode_t mode);
int rmdir_callback(const char *path);

int unlink_callback(const char *path);
int rename_callback(const char *oldPath, const char *newPath);

#endif // FUSE_HOOKS_H
