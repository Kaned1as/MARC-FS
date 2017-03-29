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

#include <fuse.h>

#include <shared_mutex>
#include <vector>

#include "object_pool.h"
#include "marc_api.h"
#include "marc_node.h"

class CloudFile;

#ifdef __APPLE__
typedef std::mutex rwMutex;
typedef std::unique_lock<rwMutex> rwLock;
#else
typedef std::shared_timed_mutex rwMutex;
typedef std::shared_lock<rwMutex> rwLock;
#endif

/**
 * @brief The MruData class - filesystem-wide metadata and caching for the mounted filesystem.
 *
 * As FUSE is multithreaded, caching and file operations all use locking quite heavily.
 * Each file-altering operation obtains a lock on specified file. In addition, cache lock
 * is taken each time new node appears/disappears/opened.
 *
 * @note It is implied that you do not alter any information on the cloud while
 *       it is mounted somewhere via MARC-FS as cache is built only once and then
 *       considered immutable.
 *
 * @see fuse_hooks.cpp
  */
class MruData {
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
    template<typename T>
    T* getNode(std::string path);

    /**
     * @brief putCacheStat - write stat information to associated file/dir
     * @note this takes/releases cache lock
     * @param path path to obtain node for
     * @param cf stat to apply
     */
    void putCacheStat(std::string path, const CloudFile *cf);

    /**
     * @brief createFile - create cached file node, possibly replacing negative cache
     * @note this takes/releases cache lock
     * @param path path to create node for
     */
    template<typename T>
    void create(std::string path);

    /**
     * @brief purgeCache - erase cache at specified path
     * @note this takes/releases cache lock
     * @param path path to clear cache for
     */
    int purgeCache(std::string path);

    /**
     * @brief renameCache - rename file in cache- leave everything intact
     * @param oldPath - old path to file
     * @param newPath - new path to file
     */
    void renameCache(std::string oldPath, std::string newPath);

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
    std::map<std::string, std::unique_ptr<MarcNode>> cache;
    /**
     * @brief cacheLock - lock for changing cache structure
     */
    rwMutex cacheLock;
};

#endif // MRU_METADATA_H
