/*****************************************************************************
 *
 * Copyright (c) 2017, Oleg `Kanedias` Chernovskiy
 *
 * This file is part of MARC-FS.
 *
 * MARC-FS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MARC-FS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MARC-FS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "marc_file_node.h"

MarcFileNode::MarcFileNode()
{
}

MarcFileNode::MarcFileNode(std::vector<char> &&data)
{
    cachedContent = std::move(data);
}

void MarcFileNode::fillStats(struct stat *stbuf) const
{
    MarcNode::fillStats(stbuf);
    stbuf->st_mode = S_IFREG | 0600; // cloud files are readable, but don't launch them
    stbuf->st_nlink = 1;
    stbuf->st_size = static_cast<off_t>(getSize()); // offset is 32 bit on x86 platforms
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

void MarcFileNode::setSize(size_t size)
{
    this->size = size;
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

size_t MarcFileNode::getSize() const
{
    if (!cachedContent.empty())
        return cachedContent.size();

    return size;
}
