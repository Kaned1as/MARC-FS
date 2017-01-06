#ifndef MARC_NODE_H
#define MARC_NODE_H

#include <fuse.h>
#include <mutex>

/**
 * @brief The MarcNode class - cache node
 */
class MarcNode
{
public:
    MarcNode();
    virtual ~MarcNode();

    time_t getLastUpdated() const;
    void setLastUpdated(const time_t &value);

    bool hasStat() const;
    struct stat getStat() const;
    void setStat(const struct stat &value);

    std::mutex& getMutex();

private:
    std::mutex mutex;

    time_t lastUpdated = 0;

    bool statSet = false;
    struct stat stat = {};
};

#endif // MARC_NODE_H
