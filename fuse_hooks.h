/*****************************************************************************
 *
 * Copyright (c) 2017, Oleg `Kanedias` Chernovskiy
 *
 * This file is part of MARC-FS.
 *
 * MARC-FS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MARC-FS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MARC-FS.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FUSE_HOOKS_H
#define FUSE_HOOKS_H

#include "mru_metadata.h"

extern MruData fsMetadata;

void * initCallback(struct fuse_conn_info *conn);

int getattrCallback(const char *path, struct stat *stbuf);
int chmodCallback (const char *path, mode_t mode);
int statfsCallback(const char *path, struct statvfs *stat);
/**
 * @note API doesn't support milliseconds, we only have seconds of mtime
 */
int utimeCallback(const char *path, struct utimbuf *utime);

int openCallback(const char *path, struct fuse_file_info *fi);
int releaseCallback(const char *path, struct fuse_file_info *fi);

/**
 * @note mknodCallback - invoked when file is absent when writing
 */
int mknodCallback(const char *path, mode_t mode, dev_t dev);
int readdirCallback(const char *path, void *dirhandle, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);

int writeCallback(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int readCallback(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int truncateCallback(const char *path, off_t size);
int flushCallback(const char *path, fuse_file_info *fi);

int mkdirCallback(const char *path, mode_t mode);
int rmdirCallback(const char *path);

int unlinkCallback(const char *path);
int renameCallback(const char *oldPath, const char *newPath);

#endif // FUSE_HOOKS_H
