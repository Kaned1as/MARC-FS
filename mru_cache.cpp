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

#include "mru_cache.h"

#include "marc_api_cloudfile.h"

#include "marc_file_node.h"
#include "marc_dir_node.h"

using namespace std;

static void fillStat(struct stat *stbuf, const CloudFile *cf) {
    auto ctx = fuse_get_context();
    stbuf->st_uid = ctx->uid; // file is always ours, as long as we're authenticated
    stbuf->st_gid = ctx->gid;

    if (cf->getType() == CloudFile::File) {
        stbuf->st_mode = S_IFREG | 0600; // cloud files are readable, but don't launch them
        stbuf->st_nlink = 1;
    } else {
        stbuf->st_mode = S_IFDIR | 0700;
        stbuf->st_nlink = 2;
    }

    stbuf->st_size = static_cast<off_t>(cf->getSize()); // offset is 32 bit on x86 platforms
    stbuf->st_blksize = 4096;
    stbuf->st_blocks = cf->getSize() / 512 + 1;
#ifndef __APPLE__
    stbuf->st_mtim.tv_sec = cf->getMtime();
#else
    stbuf->st_mtimespec.tv_sec = mtime;
#endif
}

static void emptyStat(struct stat *stbuf, int type) {
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
#ifndef __APPLE__
    stbuf->st_mtim.tv_sec = time(nullptr);
#else
    stbuf->st_mtimespec.tv_sec = time(nullptr);
#endif
}

CacheNode* MruMetadataCache::getNode(string path) {
    RwLock lock(cacheLock);
    auto it = cache.find(path);
    if (it != cache.end()) {
       return it->second.get();
    }

    return nullptr;
}

void MruMetadataCache::putCacheStat(string path, const CloudFile *cf) {
    lock_guard<RwMutex> lock(cacheLock);

    auto it = cache.find(path);
    if (it != cache.end()) // altering cache where it's already present, skip
        return; // may happen in getattr

    if (!cf) {
        // mark non-existing
        cache[path] = make_unique<CacheNode>();
        return;
    }

    auto file = new CacheNode;
    fillStat(&file->stbuf, cf);
    file->exists = true;

    cache[path] = unique_ptr<CacheNode>(file);
}

void MruMetadataCache::putCacheStat(string path, int type)
{
    auto file = new CacheNode;

    if (type == S_IFDIR || type == S_IFREG) {
        emptyStat(&file->stbuf, type);
        file->exists = true;
    }

    cache[path] = unique_ptr<CacheNode>(file);
}

void MruMetadataCache::purgeCache(string path) {
    lock_guard<RwMutex> lock(cacheLock);
    auto it = cache.find(path);

    if (it != cache.end())
        cache.erase(it);
}

void MruMetadataCache::renameCache(string oldPath, string newPath, bool reset)
{
    lock_guard<RwMutex> lock(cacheLock);
    cache[newPath].swap(cache[oldPath]);
    if (reset) {
        cache[oldPath].reset(new CacheNode);
    }

    // FIXME: moving dirs moves all their content with them!
}

bool MruMetadataCache::tryFillDir(string path, void *dirhandle, fuse_fill_dir_t filler)
{
    // we store cache in sorted map, this means we can find dir contents
    // by finding itself and promoting iterator further
    RwLock lock(cacheLock);
    auto it = cache.find(path);
    if (it == cache.end())
        return false; // not found in cache

    // make sure we did readdir on that path previously
    const auto dir = it->second.get();

    if (!dir->dir_cached)
        return false; // readdir wasn't performed

    // root dir is special, e.g.
    // "/" -> "/" but "/folder" -> "/folder/"
    bool isRoot = path == "/";
    string nestedPath = isRoot ? path : path + '/';

    // handle "folder.", "folder-" that may happen before "folder/"
    // move iterator to the point where it sees first file inside this dir
    // see issue #24
    while (true) {
        ++it;

        // reached the end of the cache, meaning this folder was last in cache and
        // we have cache entry for this dir itself but not for its contents
        if (it == cache.end())
            return false;

        if (it->first.find(path) == string::npos) {
            // we moved to other entries, meaning this dir is empty
            // return cached result
            return true;
        }

        if (it->first.find(nestedPath) == 0)
            break; // found first instance of "folder/something"
    }

    for (; it != cache.end(); ++it) {
        if (it->first.find(nestedPath) != 0) // we moved to other entries, break
            break;

        if (!it->second->exists)
            continue; // skip negative nodes

        string innerPath = it->first.substr(nestedPath.size());
        if (innerPath.find('/') != string::npos)
            continue; // skip nested directories

        filler(dirhandle, innerPath.data(), &it->second->stbuf, 0, (fuse_fill_dir_flags) 0);
    }

    return true;
}
