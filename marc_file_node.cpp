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

#include <fuse.h>
#include <algorithm>

#include "marc_api.h"
#include "marc_file_node.h"

using namespace std;

MarcFileNode::MarcFileNode()
{
}

void MarcFileNode::fillStats(struct stat *stbuf) const
{
    MarcNode::fillStats(stbuf);
    stbuf->st_mode = S_IFREG | 0600; // cloud files are readable, but don't launch them
    stbuf->st_nlink = 1;
    stbuf->st_size = static_cast<off_t>(getSize()); // offset is 32 bit on x86 platforms
}

void MarcFileNode::open(MarcRestClient *client, string path)
{
    if (newlyCreated) {
        // do nothing, await for `write`s
        return;
    }

    // not a new file, need to download
    if (fileSize > MARCFS_MAX_FILE_SIZE) {
        // compound file
        size_t partCount = (fileSize / MARCFS_MAX_FILE_SIZE) + 1;     // let's say, file is 3GB, that gives us 2 parts
        for (size_t idx = 0; idx < partCount; ++idx) {
            string extendedPathname = string(path) + MARCFS_SUFFIX + to_string(idx);
            client->download(extendedPathname, cachedContent); // append part to current cache
        }
    } else {
        // single file
        client->download(path, cachedContent);
    }
}


void MarcFileNode::flush(MarcRestClient *client, string path)
{
    // skip unchanged files
    if (!dirty && !newlyCreated)
        return;

    // cachedContent.size() now holds current size, fileSize holds old size
    if (fileSize > MARCFS_MAX_FILE_SIZE) {
        // old one was compound file - delete old parts
        size_t oldPartCount = (fileSize / MARCFS_MAX_FILE_SIZE) + 1;
        for (size_t idx = 0; idx < oldPartCount; ++idx) {
            string extendedPathname = string(path) + MARCFS_SUFFIX + to_string(idx);
            client->remove(extendedPathname);
        }
    }

    if (cachedContent.size() > MARCFS_MAX_FILE_SIZE) {
        // new one is compound - upload new parts
        size_t partCount = (cachedContent.size() / MARCFS_MAX_FILE_SIZE) + 1;
        size_t offset = 0;
        for (size_t idx = 0; idx < partCount; ++idx) {
            string extendedPathname = string(path) + MARCFS_SUFFIX + to_string(idx);
            client->upload(extendedPathname, cachedContent, offset, MARCFS_MAX_FILE_SIZE);
            offset += MARCFS_MAX_FILE_SIZE;
        }
    } else {
        // single file
        client->upload(path, cachedContent);
    }

    // cleanup
    setMtime(time(nullptr));
    dirty = false;
    newlyCreated = false;
}

int MarcFileNode::read(char *buf, size_t size, uint64_t offsetBytes)
{
    auto len = cachedContent.size();
    if (offsetBytes > len)
        return 0; // requested bytes above the size

    if (offsetBytes + size > len) {
        // requested size is more than we have
        copy_n(&cachedContent.front() + offsetBytes, len - offsetBytes, buf);
        return static_cast<int>(len - offsetBytes);
    }

    // normal operation
    copy_n(&cachedContent.front() + offsetBytes, size, buf);
    return static_cast<int>(size);
}

int MarcFileNode::write(const char *buf, size_t size, uint64_t offsetBytes)
{
    if (offsetBytes + size > cachedContent.size()) {
        cachedContent.resize(offsetBytes + size);
    }

    copy_n(buf, size, &cachedContent.front() + offsetBytes);
    dirty = true;
    return static_cast<int>(size);
}

void MarcFileNode::remove(MarcRestClient *client, string path)
{
    if (fileSize > MARCFS_MAX_FILE_SIZE) {
        // compound file, remove each part
        size_t partCount = (fileSize / MARCFS_MAX_FILE_SIZE) + 1;
        for (size_t idx = 0; idx < partCount; ++idx) {
            string extendedPathname = string(path) + MARCFS_SUFFIX + to_string(idx);
            client->remove(extendedPathname); // append part to current cache
        }
    } else {
        // single file
        client->remove(path);
    }
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
    this->fileSize = size;
}

vector<char>& MarcFileNode::getCachedContent()
{
    return cachedContent;
}

void MarcFileNode::setCachedContent(const vector<char> &value)
{
    cachedContent = value;
}

void MarcFileNode::setCachedContent(const vector<char> &&value)
{
    cachedContent = move(value);
}

size_t MarcFileNode::getSize() const
{
    if (!cachedContent.empty())
        return cachedContent.size();

    return fileSize;
}

bool MarcFileNode::isNewlyCreated() const
{
    return newlyCreated;
}

void MarcFileNode::setNewlyCreated(bool value)
{
    newlyCreated = value;
}
