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

#include "mru_metadata.h"

#include "marc_api_cloudfile.h"

#include "marc_file_node.h"
#include "marc_dir_node.h"
#include "marc_dummy_node.h"

using namespace std;

template<typename T>
LockHolder<T> MruData::getNode(string path) {
    lock_guard<mutex> lock(cacheLock);
    auto it = cache.find(path);
    if (it != cache.end()) {
        T *node = dynamic_cast<T*>(it->second.get());
        if (!node)
            throw invalid_argument("Expected another type in node " + path);

        return LockHolder<T>(node);
    }

    return LockHolder<T>();
}

// instantiations
template LockHolder<MarcNode> MruData::getNode(string path);
template LockHolder<MarcFileNode> MruData::getNode(string path);
template LockHolder<MarcDirNode> MruData::getNode(string path);
template LockHolder<MarcDummyNode> MruData::getNode(string path);

template<typename T>
void MruData::create(string path) {
    lock_guard<mutex> lock(cacheLock);
    auto it = cache.find(path);
    if (it != cache.end() && it->second->exists()) // something is already present in cache at this path!
        throw invalid_argument("Cache already contains node at path " + path);

    cache[path] = make_unique<T>();
}

// instantiations
template void MruData::create<MarcFileNode>(string path);
template void MruData::create<MarcDirNode>(string path);
template void MruData::create<MarcDummyNode>(string path);

void MruData::putCacheStat(string path, const CloudFile *cf) {
    lock_guard<mutex> lock(cacheLock);

    auto it = cache.find(path);
    if (it != cache.end()) // altering cache where it's already present, skip
        return; // may happen if getattr for absolute path happens, then readdir of parent folder

    if (!cf) {
        // mark non-existing
        cache[path] = make_unique<MarcDummyNode>();
        return;
    }

    MarcNode *node;
    switch (cf->getType()) {
        case CloudFile::File: {
            auto file = new MarcFileNode;
            file->setSize(cf->getSize());
            node = file;
            break;
        }
        case CloudFile::Directory: {
            node = new MarcDirNode;
            break;
        }
    }
    node->setMtime(static_cast<time_t>(cf->getMtime()));
    cache[path] = unique_ptr<MarcNode>(node);
}

void MruData::purgeCache(string path) {
    lock_guard<mutex> lock(cacheLock);
    auto it = cache.find(path);

    // node doesn't exist
    if (it == cache.end())
        return;

    // node exists in cache, deal with it
    // nobody can alter map now, we're holding cacheLock
    // but some thread may own file, wait for it to release
    it->second->getMutex().lock(); // obtain lock, wait if necessary
    it->second->getMutex().unlock();
    cache.erase(it);
}

bool MruData::tryFillDir(string path, void *dirhandle, fuse_fill_dir_t filler)
{
    // we store cache in map, this means we can find dir contents
    // by finding itself and promoting iterator further
    lock_guard<mutex> lock(cacheLock);
    auto it = cache.find(path);
    if (it == cache.end())
        return false; // not found in cache

    // make sure we did readdir on that path previously
    const auto dir = dynamic_cast<MarcDirNode*>(it->second.get());
    if (!dir)
        throw invalid_argument("Node at requested path is not a dir!");

    if (!dir->isCached())
        return false;

    // move iterator further as first step, we don't need
    // the directory itself in listing
    for (++it; it != cache.end(); ++it) {
        if (it->first.find(path) != 0) // we are moved to other entries, break
            break;

        if (!it->second->exists())
            continue; // skip negative nodes

        // root dir is special
        // "/folder" -> "folder", "/folder/f2" -> "f2"
        auto outerSize = path == "/" ? path.size() : path.size() + 1;

        string innerPath = it->first.substr(outerSize);
        if (innerPath.find('/') != string::npos)
            continue; // skip nested directories

        struct stat stats = {};
        it->second->fillStats(&stats);
        filler(dirhandle, innerPath.data(), &stats, 0);
    }

    return true;
}
