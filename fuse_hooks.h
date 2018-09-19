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

#include "mru_cache.h"

extern MruMetadataCache fsCache;

void * initCallback(struct fuse_conn_info *conn, struct fuse_config *cfg);

int getattrCallback(const char *patb, struct stat *stbuf, struct fuse_file_info *fi);
int chmodCallback (const char *path, mode_t mode, struct fuse_file_info *fi);
int statfsCallback(const char *path, struct statvfs *stat);
/**
 * @note API doesn't support milliseconds, we only have seconds of mtime
 */
int utimensCallback(const char *path, const struct timespec time[2], struct fuse_file_info *fi);


/**
 * @note mknodCallback - invoked when file is absent when writing
 */
int mknodCallback(const char *path, mode_t mode, dev_t dev);

// directory-related
int opendirCallback(const char *path, struct fuse_file_info *fi);
int readdirCallback(const char *path, void *dirhandle, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);
int releasedirCallback(const char *path, struct fuse_file_info *fi);

// file-related
int openCallback(const char *path, struct fuse_file_info *fi);
int readCallback(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int writeCallback(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int flushCallback(const char *path, fuse_file_info *fi);
int releaseCallback(const char *path, struct fuse_file_info *fi);
int truncateCallback(const char *path, off_t size, struct fuse_file_info *fi);

int mkdirCallback(const char *path, mode_t mode);
int rmdirCallback(const char *path);

int unlinkCallback(const char *path);
int renameCallback(const char *oldPath, const char *newPath, unsigned int flags);

#endif // FUSE_HOOKS_H
