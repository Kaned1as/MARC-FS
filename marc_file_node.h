#ifndef MARC_FILE_NODE_H
#define MARC_FILE_NODE_H

#include <vector>

#include "marc_node.h"

class MarcFileNode : public MarcNode
{
public:
    MarcFileNode();
    MarcFileNode(std::vector<char> &&data);

    uint64_t getTransferred() const;
    void setTransferred(const uint64_t &value);

    bool isDirty() const;
    void setDirty(bool value);

    std::vector<char>& getCachedContent();
    void setCachedContent(const std::vector<char> &value);
    void setCachedContent(const std::vector<char> &&value);

private:
    /**
     * @brief cachedContent - used for small files and random writes/reads
     *
     * If file is read/written at random locations, we try to cache it fully before
     * doing operations.
     */
    std::vector<char> cachedContent;
    /**
     * @brief transferred - number of bytes transferred so far. Needed for
     *        sequential transfer, see @ref MarcFileNode.transfer field.
     */
    uint64_t transferred = 0;
    /**
     * @brief dirty - used to indicate whether subsequent upload is needed
     */
    bool dirty = false;
};

#endif // MARC_FILE_NODE_H
