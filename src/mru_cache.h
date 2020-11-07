/*****************************************************************************
 *
 * Copyright (c) 2018-2020, Oleg `Kanedias` Chernovskiy
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
#include <memory>
#include <chrono>
#include <string>
#include <map>


#include "object_pool.h"
#include "marc_rest_client.h"
#include "marc_file_node.h"

class CloudFile;

using namespace std::chrono_literals;

struct CacheNode {

    explicit CacheNode(const struct stat &stbuf)
    : stbuf(stbuf),
      cached_since(std::chrono::steady_clock::now()) {
    }

    /**
     * @brief stbuf = Stat cache, main cache entity
     *
     */
    struct stat stbuf = {};

 private:
    /**
     * @brief cached_since - marks time when this node was created
     */
    std::chrono::time_point<std::chrono::steady_clock> cached_since;

    friend class CacheManager;
};

/**
 * 
 */
class CacheManager {
 public:
    using SharedLock = std::shared_lock<std::shared_timed_mutex>;
    using UniqueLock = std::unique_lock<std::shared_timed_mutex>;

    static CacheManager * getInstance() {
        static CacheManager instance;
        return &instance;
    }

    /**
     * 
     */
    void put(const std::string &path, const CacheNode &node);

    /**
     *
     */
    std::shared_ptr<CacheNode> get(const std::string &path);

    /**
     * 
     */
    void update(const std::string &path, MarcNode &node);
    void remove(const std::string &path);
 private:
    std::shared_timed_mutex cacheLock;

    std::chrono::seconds cacheTtl = 60s;
    std::map<std::string, std::shared_ptr<CacheNode>> statCache;
};

void emptyStat(struct stat *stbuf, int type);
void fillStat(struct stat *stbuf, const CloudFile *cf);

#endif  // MRU_METADATA_H
