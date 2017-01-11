#include "mru_metadata.h"

MarcFileNode *MruData::getOrCreateFile(std::__cxx11::string path) {
    std::lock_guard<std::mutex> lock(cacheLock);
    return getOrCreateNode(path);
}

MarcFileNode *MruData::getFile(std::__cxx11::string path) {
    std::lock_guard<std::mutex> lock(cacheLock);
    return getNode(path);
}

void MruData::putCacheStat(std::__cxx11::string path, const struct stat *stat) {
    std::lock_guard<std::mutex> lock(cacheLock);
    auto node = getOrCreateNode(path);
    std::unique_lock<std::mutex> unlocker(node->getMutex(), std::adopt_lock);
    node->setStat(*stat);
}

void MruData::purgeCache(std::__cxx11::string path) {
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
