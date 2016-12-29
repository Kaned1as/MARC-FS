#ifndef MRU_NODE_H
#define MRU_NODE_H

#include <vector>

#include "blocking_queue.h"

class MruNode
{
    using Pipe = BlockingQueue<std::vector<char>>;
public:
    MruNode();
    MruNode(std::vector<char> &&data);

    Pipe& getTransfer();
    void setTransfer(const Pipe &value);

    uint64_t getTransferred() const;
    void setTransferred(const uint64_t &value);

    bool isDirty() const;
    void setDirty(bool value);

    std::vector<char>& getCachedContent();
    void setCachedContent(const std::vector<char> &value);
    void setCachedContent(const std::vector<char> &&value);

private:
    /**
     * @brief transfer field - used for long-running sequential writes
     *        or reads of this node.
     *
     * We try to determine, whether file is read/written randomly or sequentially
     * if latter, we don't load it fully in memory but use Pipe class and async API
     * calls to transfer it efficiently.
     */
    std::unique_ptr<Pipe> transfer;
    /**
     * @brief cachedContent - used for small files and random writes/reads
     *
     * If file is read/written at random locations, we try to cache it fully before
     * doing operations.
     */
    std::vector<char> cachedContent;
    /**
     * @brief transferred - number of bytes transferred so far. Needed for
     *        sequential transfer, see @ref MruNode.transfer field.
     */
    uint64_t transferred;
    /**
     * @brief dirty - used to indicate whether subsequent upload is needed
     */
    bool dirty;
};

#endif // MRU_NODE_H
