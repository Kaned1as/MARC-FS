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
#include "mru_metadata.h"
#include "marc_file_node.h"
#include "memory_storage.h"
#include "file_storage.h"

using namespace std;

extern MruData fsMetadata;

// template instantiation declarations
extern template void MarcRestClient::upload(string remotePath, AbstractStorage &body, size_t start, size_t count);
extern template void MarcRestClient::download(string remotePath, AbstractStorage& target);

MarcFileNode::MarcFileNode()
{
    if (fsMetadata.cacheDir.empty()) {
        cachedContent.reset(new MemoryStorage);
    } else {
        cachedContent.reset(new FileStorage);
    }
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
    // initialize storage
    cachedContent->open();

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
            client->download(extendedPathname, *cachedContent); // append part to current cache
        }
    } else {
        // single file
        client->download(path, *cachedContent);
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

    if (cachedContent->size() > MARCFS_MAX_FILE_SIZE) {
        // new one is compound - upload new parts
        size_t partCount = (cachedContent->size() / MARCFS_MAX_FILE_SIZE) + 1;
        size_t offset = 0;
        for (size_t idx = 0; idx < partCount; ++idx) {
            string extendedPathname = string(path) + MARCFS_SUFFIX + to_string(idx);
            client->upload(extendedPathname, *cachedContent, offset, MARCFS_MAX_FILE_SIZE);
            offset += MARCFS_MAX_FILE_SIZE;
        }
    } else {
        // single file
        client->upload(path, *cachedContent);
    }

    // cleanup
    setMtime(time(nullptr));
    dirty = false;
    newlyCreated = false;
}

int MarcFileNode::read(char *buf, size_t size, uint64_t offsetBytes)
{
    return cachedContent->read(buf, size, offsetBytes);
}

int MarcFileNode::write(const char *buf, size_t size, uint64_t offsetBytes)
{
    int res = cachedContent->write(buf, size, offsetBytes);
    if (res > 0)
        dirty = true;
    return res;
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

void MarcFileNode::truncate(off_t size)
{
    cachedContent->truncate(size);
    dirty = true;
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

AbstractStorage& MarcFileNode::getCachedContent()
{
    return *cachedContent.get();
}

size_t MarcFileNode::getSize() const
{
    if (!cachedContent->empty())
        return cachedContent->size();

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
