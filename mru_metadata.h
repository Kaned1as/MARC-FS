#ifndef MRU_METADATA_H
#define MRU_METADATA_H

#include <map>
#include <vector>

#include "object_pool.h"
#include "marc_api.h"
#include "marc_node.h"

class CloudFile;

/**
 * RAII-style helper class to wrap file-based locking transparently. On creation the instance obtains
 * the lock of contained file, on destruction the lock is released.
 *
 * @see MruData::getNode
 * @see fuse_hooks.cpp
 */
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

private:
    T* content;
    std::unique_lock<std::mutex> scopeLock;
};

/**
 * @brief The MruData class - filesystem-wide metadata and caching for the mounted filesystem.
 *
 * As FUSE is multithreaded, caching and file operations all use locking quite heavily.
 * Each file-altering operation obtains a lock on specified file. In addition, cache lock
 * is taken each time new node appears/disappears/opened.
 *
 * @note It is implied that you do not alter any information on the cloud while
 *       it is mounted somewhere via MARC-FS as cache is built only once and then
 *       considered immutable.
 *
 * @see fuse_hooks.cpp
  */
class MruData {
public:
    ObjectPool<MarcRestClient> clientPool;

    /**
     * @brief getNode - retrieves node if it exists and of the requested type
     *        of node is of some other type, throws.
     *
     * @note this obtains cache lock for a short moment to retrieve the node,
     *       then the returned object obtains file lock and cache lock
     *       is dropped after that. File lock is dropped when object returned is
     *       no longer in use
     *
     * @param path - path to a file
     * @return ptr to obtained file node
     */
    template<typename T>
    LockHolder<T> getNode(std::string path);

    /**
     * @brief putCacheStat - write stat information to associated file/dir
     * @note this takes/releases cache lock
     * @param path path to obtain node for
     * @param cf stat to apply
     */
    void putCacheStat(std::string path, const CloudFile *cf);

    /**
     * @brief createFile - create cached file node, possibly replacing negative cache
     * @note this takes/releases cache lock
     * @param path path to create node for
     */
    template<typename T>
    void create(std::string path);

    /**
     * @brief purgeCache - erase cache at specified path
     * @note this takes/releases cache lock
     * @param path path to clear cache for
     */
    void purgeCache(std::string path);
private:

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
