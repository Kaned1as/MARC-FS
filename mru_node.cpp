#include "mru_node.h"

MruNode::MruNode()
{
    transfer.reset(new BlockingQueue<char>);
}

MruNode::MruNode(std::vector<char> &&data)
{
    cachedContent = std::move(data);
    transfer.reset(new BlockingQueue<char>);
}

MruNode::Pipe& MruNode::getTransfer()
{
    return (*transfer);
}

uint64_t MruNode::getTransferred() const
{
    return transferred;
}

void MruNode::setTransferred(const uint64_t &value)
{
    transferred = value;
}

bool MruNode::isDirty() const
{
    return dirty;
}

void MruNode::setDirty(bool value)
{
    dirty = value;
}

std::vector<char>& MruNode::getCachedContent()
{
    return cachedContent;
}

void MruNode::setCachedContent(const std::vector<char> &value)
{
    cachedContent = value;
}

void MruNode::setCachedContent(const std::vector<char> &&value)
{
    cachedContent = std::move(value);
}
