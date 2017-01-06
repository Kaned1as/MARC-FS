#include "marc_file_node.h"

MarcFileNode::MarcFileNode()
{
}

MarcFileNode::MarcFileNode(std::vector<char> &&data)
{
    cachedContent = std::move(data);
}

uint64_t MarcFileNode::getTransferred() const
{
    return transferred;
}

void MarcFileNode::setTransferred(const uint64_t &value)
{
    transferred = value;
}

bool MarcFileNode::isDirty() const
{
    return dirty;
}

void MarcFileNode::setDirty(bool value)
{
    dirty = value;
}

std::vector<char>& MarcFileNode::getCachedContent()
{
    return cachedContent;
}

void MarcFileNode::setCachedContent(const std::vector<char> &value)
{
    cachedContent = value;
}

void MarcFileNode::setCachedContent(const std::vector<char> &&value)
{
    cachedContent = std::move(value);
}
