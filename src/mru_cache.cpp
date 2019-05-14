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

#include "mru_cache.h"

#include "marc_api_cloudfile.h"

#include "marc_file_node.h"
#include "marc_dir_node.h"

using namespace std;

std::shared_ptr<CacheNode> CacheManager::get(const std::string &path) {
    SharedLock guard(cacheLock);
    
    auto cached = statCache.find(path);
    if (cached == statCache.end()) {
        return std::shared_ptr<CacheNode>();
    }

    auto now = std::chrono::steady_clock::now();
    if (cached->second->cached_since + this->cacheTtl < now) {
        // cache expired, invalidate
        guard.unlock();
        UniqueLock writeGuard(cacheLock);

        statCache.erase(cached);
        return std::shared_ptr<CacheNode>();
    }

    return cached->second;
}

void CacheManager::put(const std::string &path, const CacheNode &node) {
    UniqueLock guard(cacheLock);
    
    statCache[path] = std::make_shared<CacheNode>(node);
}

void CacheManager::update(const std::string &path, MarcNode &node) {
    UniqueLock guard(cacheLock);

    auto cached = statCache.find(path);
    if (cached == statCache.end()) {
        return;
    }

    node.fillStat(&cached->second->stbuf);
}

void fillStat(struct stat *stbuf, const CloudFile *cf) {
    auto ctx = fuse_get_context();
    stbuf->st_uid = ctx->uid; // file is always ours, as long as we're authenticated
    stbuf->st_gid = ctx->gid;

    if (cf->getType() == S_IFREG) {
        stbuf->st_mode = S_IFREG | 0600; // cloud files are readable, but don't launch them
        stbuf->st_nlink = 1;
    } else {
        stbuf->st_mode = S_IFDIR | 0700;
        stbuf->st_nlink = 2;
    }

    stbuf->st_size = static_cast<off_t>(cf->getSize()); // offset is 32 bit on x86 platforms
    stbuf->st_blksize = 4096;
    stbuf->st_blocks = cf->getSize() / 512 + 1;
    stbuf->st_mtim.tv_sec = cf->getMtime();
}

void emptyStat(struct stat *stbuf, int type) {
    auto ctx = fuse_get_context();
    stbuf->st_uid = ctx->uid; // file is always ours, as long as we're authenticated
    stbuf->st_gid = ctx->gid;

    if (type == S_IFREG) {
        stbuf->st_mode = S_IFREG | 0600;
        stbuf->st_nlink = 1;
    } else {
        stbuf->st_mode = S_IFDIR | 0700;
        stbuf->st_nlink = 2;
    }

    stbuf->st_size = 0;
    stbuf->st_blksize = 4096;
    stbuf->st_blocks = 1;
    stbuf->st_mtim.tv_sec = time(nullptr);
}