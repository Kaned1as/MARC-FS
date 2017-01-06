#include "marc_node.h"

static const time_t STAT_CACHE_SEC = 20;

MarcNode::MarcNode()
{

}

MarcNode::~MarcNode()
{

}

time_t MarcNode::getLastUpdated() const
{
    return lastUpdated;
}

void MarcNode::setLastUpdated(const time_t &value)
{
    lastUpdated = value;
}

bool MarcNode::hasStat() const
{
    if (!statSet)
        return false;

    // check expiration also
    time_t current = std::time(nullptr);
    return current - lastUpdated < STAT_CACHE_SEC;
}

struct stat MarcNode::getStat() const
{
    return stat;
}

void MarcNode::setStat(const struct stat &value)
{
    lastUpdated = std::time(nullptr);
    statSet = true;
    stat = value;
}

std::mutex& MarcNode::getMutex()
{
    return mutex;
}
