#include "mru_metadata.h"

#include "marc_api_cloudfile.h"

#include "marc_file_node.h"
#include "marc_dir_node.h"
#include "marc_dummy_node.h"

template<typename T>
LockHolder<T> MruData::getNode(std::string path) {
    std::lock_guard<std::mutex> lock(cacheLock);
    auto it = cache.find(path);
    if (it != cache.end()) {
        T *node = dynamic_cast<T*>(it->second.get());
        if (!node)
            throw std::invalid_argument("Expected another type in node " + path);

        return LockHolder<T>(node);
    }

    return LockHolder<T>();
}

// instantiations
template LockHolder<MarcNode> MruData::getNode(std::string path);
template LockHolder<MarcFileNode> MruData::getNode(std::string path);
template LockHolder<MarcDirNode> MruData::getNode(std::string path);
template LockHolder<MarcDummyNode> MruData::getNode(std::string path);

template<typename T>
void MruData::create(std::string path) {
    std::lock_guard<std::mutex> lock(cacheLock);
    auto it = cache.find(path);
    if (it != cache.end() && it->second->exists()) // something is already present in cache at this path!
        throw std::invalid_argument("Cache already contains node at path " + path);

    cache[path] = std::make_unique<T>();
}

// instantiations
template void MruData::create<MarcFileNode>(std::string path);
template void MruData::create<MarcDirNode>(std::string path);
template void MruData::create<MarcDummyNode>(std::string path);

void MruData::putCacheStat(std::string path, const CloudFile *cf) {
    std::lock_guard<std::mutex> lock(cacheLock);

    auto it = cache.find(path);
    if (it != cache.end()) // altering cache where it's already present, skip
        return;

    if (!cf) {
        // mark non-existing
        cache[path] = std::make_unique<MarcDummyNode>();
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
    cache[path] = std::unique_ptr<MarcNode>(node);
}

void MruData::purgeCache(std::string path) {
    std::lock_guard<std::mutex> lock(cacheLock);
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
