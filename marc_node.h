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

    void setMtime(time_t time);

    virtual bool exists() const;
    virtual void fillStats(struct stat *stbuf) const;

    std::mutex& getMutex();

private:
    std::mutex mutex;

    time_t mtime = 0;
};

#endif // MARC_NODE_H
