#ifndef MRU_METADATA_H
#define MRU_METADATA_H

#include <map>
#include <vector>
#include "object_pool.h"
#include "marc_api.h"
#include "marc_file_node.h"

class MruData {
public:
    MruData() : clientPool(0) {

    }

    ObjectPool<MarcRestClient> clientPool;

    /**
     * @brief obtainFile - do everything needed to create node at specified
     *        path and lock it to our thread.
     * @param path - path to a file
     * @return ptr to obtained file node
     */
    MarcFileNode* getOrCreateFile(std::string path) {
        std::lock_guard<std::mutex> lock(cacheLock);
        return getOrCreateNode(path);
    }

    /**
     * @brief getFile - retrieve node if exists
     * @param path - path to a file
     * @return
     */
    MarcFileNode* getFile(std::string path) {
        std::lock_guard<std::mutex> lock(cacheLock);
        return getNode(path);
    }

    /**
     * @brief putCacheStat - write stat information to associated file/dir
     * @param path path to obtain node for
     * @param stat stat struct to apply
     */
    void putCacheStat(std::string path, const struct stat *stat) {
        std::lock_guard<std::mutex> lock(cacheLock);
        auto node = getOrCreateNode(path);
        std::unique_lock<std::mutex> unlocker(node->getMutex(), std::adopt_lock);
        node->setStat(*stat);
    }

    void purgeCache(std::string path) {
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
private:
    /**
     * @brief getNode - retrieves cached node or creates it,
     *        locking its mutex to calling thread
     * @note called with a @ref cacheLock held
     * @param path path to query
     * @return filenode pointer
     */
    MarcFileNode* getOrCreateNode(std::string path) {
        auto it = cache.find(path);
        if (it == cache.end()) {
            cache.emplace(path, std::make_unique<MarcFileNode>());
        }
        auto &node = cache[path];
        node->getMutex().lock();
        return node.get();
    }

    MarcFileNode* getNode(std::string path) {
        auto it = cache.find(path);
        if (it != cache.end()) {
            it->second->getMutex().lock();
            return it->second.get();
        }
        return nullptr;
    }

    /**
     * @brief cache field - cached filenodes
     */
    std::map<std::string, std::unique_ptr<MarcFileNode>> cache;
    std::mutex cacheLock;
};

#endif // MRU_METADATA_H
