/*****************************************************************************
 *
 * Copyright (c) 2018-2019, Oleg `Kanedias` Chernovskiy
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

typedef std::shared_timed_mutex RwMutex;
typedef std::shared_lock<RwMutex> RwLock;

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

//    inline time_t getMtime() {
//        return retrieveMtime(stbuf);
//    }

//    inline void setMtime(time_t mtime) {
//        applyMtime(stbuf, mtime);
//    }
};

void emptyStat(struct stat *stbuf, int type);
void fillStat(struct stat *stbuf, const CloudFile *cf);

#endif // MRU_METADATA_H
