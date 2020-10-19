/*****************************************************************************
 *
 * Copyright (c) 2018-2019, Oleg `Kanedias` Chernovskiy
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

#include <algorithm>

#include "marc_rest_client.h"
#include "mru_cache.h"
#include "marc_file_node.h"
#include "memory_storage.h"
#include "file_storage.h"

extern std::string cacheDir;

MarcFileNode::MarcFileNode() {
    if (cacheDir.empty()) {
        cachedContent.reset(new MemoryStorage);
    } else {
        cachedContent.reset(new FileStorage);
    }
}

MarcFileNode::MarcFileNode(const struct stat &stbuf) : MarcFileNode() {
    // required to understand whether this file is compound or not
    oldFileSize = stbuf.st_size;
    mtime = stbuf.st_mtim.tv_sec;
}

void MarcFileNode::fillStat(struct stat *stbuf) {
    MarcNode::fillStat(stbuf);

    stbuf->st_mode = S_IFREG | 0600; // cloud files are readable, but don't launch them
    stbuf->st_nlink = 1;

    stbuf->st_size = static_cast<off_t>(getSize()); // offset is 32 bit on x86 platforms
    stbuf->st_blocks = getSize() / 512 + 1;
}

void MarcFileNode::open(MarcRestClient *client, std::string path) {
    // open is potentially network-download operation, lock it
    std::unique_lock<std::mutex> guard(netMutex);

    if (opened) {
        // shouldn't happen
        return;
    }

    // initialize storage
    cachedContent->open();
    opened = true;

    // not a new file, need to download
    if (oldFileSize > MARCFS_MAX_FILE_SIZE) {
        // compound file
        off_t partCount = (oldFileSize / MARCFS_MAX_FILE_SIZE) + 1;     // let's say, file is 3GB, that gives us 2 parts
        for (off_t idx = 0; idx < partCount; ++idx) {
            std::string extendedPathname = std::string(path) + MARCFS_SUFFIX + std::to_string(idx);
            client->download(extendedPathname, *cachedContent); // append part to current cache
        }
    } else {
        // single file
        client->download(path, *cachedContent);
    }
}


void MarcFileNode::flush(MarcRestClient *client, std::string path) {
    // open is potentially network-upload operation, lock it
    std::unique_lock<std::mutex> guard(netMutex);

    // skip unchanged files
    if (!dirty)
        return;

    // cachedContent.size() now holds current size, fileSize holds old size
    if (oldFileSize > MARCFS_MAX_FILE_SIZE) {
        // old one was compound file - delete old parts
        off_t oldPartCount = (oldFileSize / MARCFS_MAX_FILE_SIZE) + 1;
        for (off_t idx = 0; idx < oldPartCount; ++idx) {
            std::string extendedPathname = std::string(path) + MARCFS_SUFFIX + std::to_string(idx);
            client->remove(extendedPathname);
        }
    } else {
        // old one was regular non-compound one, delete it
        client->remove(path);
    }

    if (cachedContent->size() > MARCFS_MAX_FILE_SIZE) {
        // new one is compound - upload new parts
        off_t partCount = (cachedContent->size() / MARCFS_MAX_FILE_SIZE) + 1;
        off_t offset = 0;
        for (off_t idx = 0; idx < partCount; ++idx) {
            std::string extendedPathname = std::string(path) + MARCFS_SUFFIX + std::to_string(idx);
            client->upload(extendedPathname, *cachedContent, offset, MARCFS_MAX_FILE_SIZE);
            offset += MARCFS_MAX_FILE_SIZE;
        }
    } else {
        // single file
        client->upload(path, *cachedContent);
    }

    // cleanup
    dirty = false;
    oldFileSize = cachedContent->size();
}

int MarcFileNode::read(char *buf, size_t size, uint64_t offsetBytes) {
    return cachedContent->read(buf, size, offsetBytes);
}

int MarcFileNode::write(const char *buf, size_t size, uint64_t offsetBytes) {
    int res = cachedContent->write(buf, size, offsetBytes);
    if (res > 0) {
        dirty = true;
        mtime = time(nullptr);
    }
    return res;
}

void MarcFileNode::remove(MarcRestClient *client, std::string path) {
    if (oldFileSize > MARCFS_MAX_FILE_SIZE) {
        // compound file, remove each part
        off_t partCount = (oldFileSize / MARCFS_MAX_FILE_SIZE) + 1;
        for (off_t idx = 0; idx < partCount; ++idx) {
            std::string extendedPathname = std::string(path) + MARCFS_SUFFIX + std::to_string(idx);
            client->remove(extendedPathname); // append part to current cache
        }
    } else {
        // single file
        client->remove(path);
    }
}

void MarcFileNode::rename(MarcRestClient *client, std::string oldPath, std::string newPath) {
    if (oldFileSize > MARCFS_MAX_FILE_SIZE) {
        // compound file, move each part
        off_t partCount = (oldFileSize / MARCFS_MAX_FILE_SIZE) + 1;
        for (off_t idx = 0; idx < partCount; ++idx) {
            std::string oldExtPathname = std::string(oldPath) + MARCFS_SUFFIX + std::to_string(idx);
            std::string newExtPathname = std::string(newPath) + MARCFS_SUFFIX + std::to_string(idx);
            client->rename(oldExtPathname, newExtPathname);
        }
    } else {
        // single file, common approach
        MarcNode::rename(client, oldPath, newPath);
    }
}

void MarcFileNode::truncate(off_t size) {
    cachedContent->truncate(size);
    // truncate doesn't open file, set size accordingly
    oldFileSize = size;
    dirty = true;
}

void MarcFileNode::release() {
    // this is called after all threads released the file
    std::unique_lock<std::mutex> guard(netMutex);
    oldFileSize = cachedContent->size(); // set cached size to last content size before clearing
    cachedContent->clear(); // forget contents of a node
    opened = false;
}

off_t MarcFileNode::getSize() const {
    if (!cachedContent->empty())
        return cachedContent->size();

    return oldFileSize;
}

time_t MarcFileNode::getMtime() const {
    return mtime;
}

void MarcFileNode::setMtime(time_t time) {
    mtime = time;
}

bool MarcFileNode::isOpen() const {
    return opened;
}
