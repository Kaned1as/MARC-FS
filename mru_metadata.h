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
    MarcFileNode* getOrCreateFile(std::string path);

    /**
     * @brief getFile - retrieve node if exists
     * @param path - path to a file
     * @return
     */
    MarcFileNode* getFile(std::string path);

    /**
     * @brief putCacheStat - write stat information to associated file/dir
     * @param path path to obtain node for
     * @param stat stat struct to apply
     */
    void putCacheStat(std::string path, const struct stat *stat);

    void purgeCache(std::string path);
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
