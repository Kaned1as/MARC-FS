#include "mru_metadata.h"

#include "marc_api_cloudfile.h"

LockHolder<MarcFileNode> MruData::getOrCreateFile(std::string path) {
    std::lock_guard<std::mutex> lock(cacheLock);
    auto file = getOrCreateFileNode(path);

    return LockHolder<MarcFileNode>(file);
}

LockHolder<MarcDirNode> MruData::getOrCreateDir(std::string path) {
    std::lock_guard<std::mutex> lock(cacheLock);
    auto dir = getOrCreateDirNode(path);

    dir->getMutex().lock();
    return LockHolder<MarcDirNode>(dir);
}

void MruData::putCacheStat(std::string path, const CloudFile *cf) {
    std::lock_guard<std::mutex> lock(cacheLock);
    if (!cf) {
        // mark non-existing
        purgeNode(path);
        cache[path] = std::make_unique<MarcDummyNode>();
        return;
    }

    MarcNode *node;
    switch (cf->getType()) {
        case CloudFile::File: {
            auto file = getOrCreateFileNode(path);
            file->setSize(cf->getSize());
            node = file;
            break;
        }
        case CloudFile::Directory: {
            node = getOrCreateDirNode(path);
            break;
        }
    }
    node->setMtime(static_cast<time_t>(cf->getMtime()));
}

void MruData::purgeCache(std::string path) {
    std::lock_guard<std::mutex> lock(cacheLock);
    purgeNode(path);
}

void MruData::purgeNode(std::string path)
{
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
