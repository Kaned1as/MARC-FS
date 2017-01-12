#ifndef MRU_METADATA_H
#define MRU_METADATA_H

#include <map>
#include <vector>

#include "object_pool.h"
#include "marc_api.h"
#include "marc_file_node.h"
#include "marc_dir_node.h"
#include "marc_dummy_node.h"

class CloudFile;

template <typename T>
struct LockHolder {
    LockHolder()
        : content(nullptr), scopeLock()
    {
    }

    explicit LockHolder(T* node)
        : content(node), scopeLock(node->getMutex())
    {
    }

    T* operator->() const noexcept {
      return content;
    }

    operator bool() const {
        return content;
    }

    T* content;
    std::unique_lock<std::mutex> scopeLock;
};

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
    LockHolder<MarcFileNode> getOrCreateFile(std::string path);
    LockHolder<MarcDirNode> getOrCreateDir(std::string path);

    template<typename T>
    LockHolder<T> getNode(std::string path) {
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

    /**
     * @brief putCacheStat - write stat information to associated file/dir
     * @param path path to obtain node for
     * @param stat stat struct to apply
     */
    void putCacheStat(std::string path, const CloudFile *cf);

    void purgeCache(std::string path);
private:

    MarcFileNode* getOrCreateFileNode(std::string path) {
        auto it = cache.find(path);
        if (it == cache.end()) {
            cache.emplace(path, std::make_unique<MarcFileNode>());
        }
        auto node = cache[path].get();
        return dynamic_cast<MarcFileNode*>(node);
    }

    MarcDirNode* getOrCreateDirNode(std::string path) {
        auto it = cache.find(path);
        if (it == cache.end()) {
            cache.emplace(path, std::make_unique<MarcDirNode>());
        }
        auto node = cache[path].get();
        return dynamic_cast<MarcDirNode*>(node);
    }

    void purgeNode(std::string path);

    /**
     * @brief cache field - cached filenodes
     */
    std::map<std::string, std::unique_ptr<MarcNode>> cache;
    /**
     * @brief cacheLock - lock for changing cache structure
     */
    std::mutex cacheLock;
};

#endif // MRU_METADATA_H
