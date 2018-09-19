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

#ifndef MRU_METADATA_H
#define MRU_METADATA_H

#include <fuse3/fuse.h>

#include <shared_mutex>
#include <vector>

#include "object_pool.h"
#include "marc_rest_client.h"
#include "marc_file_node.h"

class CloudFile;

#ifdef __APPLE__
typedef std::mutex RwMutex;
typedef std::unique_lock<RwMutex> RwLock;
#else
typedef std::shared_timed_mutex RwMutex;
typedef std::shared_lock<RwMutex> RwLock;
#endif

static time_t retrieveMtime(struct stat &stbuf) {
#ifndef __APPLE__
    return stbuf.st_mtim.tv_sec;
#else
    return stbuf.st_mtimespec.tv_sec;
#endif
}

static void applyMtime(struct stat &stbuf, time_t mtime) {
#ifndef __APPLE__
    stbuf.st_mtim.tv_sec = mtime;
#else
    stbuf.st_mtimespec.tv_sec = mtime;
#endif
}

struct CacheNode {
    /**

     * @brief stbuf = Stat cache, main cache entity
     *
     */
    struct stat stbuf = {};

    /**
     * @brief exists - by default, cache node doesn't exist
     */
    bool exists = false;

    /**
     * @brief dir_cached - true if we this cache node is a dir and was read at least once
     */
    bool dir_cached = false;

    /**
     * @brief cached_since - marks time when this node was created
     */
    time_t cached_since = time(nullptr);

    inline off_t getSize() {
        return stbuf.st_size;
    }

    inline void setSize(off_t size) {
        stbuf.st_size = size;
    }

    inline time_t getMtime() {
        return retrieveMtime(stbuf);
    }

    inline void setMtime(time_t mtime) {
        applyMtime(stbuf, mtime);
    }
};

/**
 * @brief The MruData class - filesystem-wide metadata and caching for the mounted filesystem.
 *
 * As FUSE is multithreaded, caching and file operations all use locking quite heavily.
 * Each file-altering operation obtains a lock on specified file. In addition, cache lock
 * is taken each time new node appears/disappears/opened.
 *
 * @see fuse_hooks.cpp
  */
class MruMetadataCache {
public:
    ObjectPool<MarcRestClient> clientPool;
    /**
     * @brief cacheDir - cache directory, is set only on option parsing,
     *        constant everywhere else in runtime
     */
    std::string cacheDir;

    /**
     * @brief getNode - retrieves node if it exists and of the requested type
     *        of node is of some other type, throws.
     *
     * @note this obtains cache lock for a short moment to retrieve the node,
     *       then the returned object obtains file lock and cache lock
     *       is dropped after that. File lock is dropped when object returned is
     *       no longer in use
     *
     * @param path - path to a file
     * @return ptr to obtained file node
     */
    CacheNode* getNode(std::string path);

    /**
     * @brief putCacheStat - write stat information to associated file/dir
     * @note this takes/releases cache lock
     * @param path path to obtain node for
     * @param cf stat to apply, can be nullptr for negative cache
     */
    void putCacheStat(std::string path, const CloudFile *cf);

    /**
     * @brief createFile - create cached file node, possibly replacing negative cache
     * @note this takes/releases cache lock
     * @param path path to create node for
     */
    void putCacheStat(std::string path, int type);

    /**
     * @brief purgeCache - erase cache at specified path
     * @note this takes/releases cache lock
     * @param path path to clear cache for
     */
    void purgeCache(std::string path);

    /**
     * @brief renameCache - rename file in cache - leave everything other intact
     * @param oldPath - old path to file
     * @param newPath - new path to file
     */
    void renameCache(std::string oldPath, std::string newPath, bool reset = true);

    /**
     * @brief tryFillDir - tries to fill FUSE directory handle with cached content
     * @param path path to dir to fill contents for
     * @param dirhandle - passed from readdir
     * @param filler - passed from readdir
     * @return true if directory was found in cache, false otherwise
     * @see fuse_hooks.cpp
     */
    bool tryFillDir(std::string path, void *dirhandle, fuse_fill_dir_t filler);
private:

    /**
     * @brief cache field - cached filenodes
     */
    std::map<std::string, std::unique_ptr<CacheNode>> cache;

    /**
     * @brief cacheLock - lock for changing cache structure
     */
    RwMutex cacheLock;
};

#endif // MRU_METADATA_H
