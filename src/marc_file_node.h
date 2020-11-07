/*****************************************************************************
 *
 * Copyright (c) 2018-2020, Oleg `Kanedias` Chernovskiy
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

#ifndef MARC_FILE_NODE_H
#define MARC_FILE_NODE_H

#include <vector>
#include <memory>

#include "marc_node.h"
#include "abstract_storage.h"

class MarcRestClient;
class CacheNode;

/**
 * @brief The MarcFileNode class - opened node of regular file on the cloud
 */
class MarcFileNode : public MarcNode
{
public:
    MarcFileNode();
    explicit MarcFileNode(const struct stat &stbuf);

    void open(MarcRestClient *client, std::string path);
    void flush(MarcRestClient *client, std::string path);
    int read(char *buf, size_t size, uint64_t offsetBytes);
    int write(const char *buf, size_t size, uint64_t offsetBytes);
    void remove(MarcRestClient *client, std::string path);
    void rename(MarcRestClient *client, std::string oldPath, std::string newPath) override;
    void fillStat(struct stat *stbuf) override;
    void truncate(off_t size);
    void release();

    off_t getSize() const;
    time_t getMtime() const;
    void setMtime(time_t mtime);

    bool isOpen() const;

private:
    /**
     * @brief cachedContent - backing storage for open-write/read-release sequence
     */
    std::unique_ptr<AbstractStorage> cachedContent;

    /**
     * @brief dirty - used to indicate whether subsequent upload is needed
     */
    bool dirty = false;

    /**
     * @brief oldFileSize - holds size that was passed from cloud (or sum of compounds)
     */
    off_t oldFileSize = 0;

    /**
     * @brief mtime - modification time of this file
     */
    time_t mtime = time(nullptr);

    /**
     * @brief netMutex - track actual network usage by this file.
     *
     * If many readers/writers will try to open this file concurrently,
     * it may result in multiple download requests, certainly that's not what
     * we want. Similarly, concurrent flush requests would cause multiple uploads.
     *
     * Lock this in case some do-once network operation is possible.
     */
    mutable std::mutex netMutex;

    /**
     * @brief opened - indicated whether the file is already open by
     *                 you or some another thread
     *
     * Guarded by mutex @ref netMutex
     */
    bool opened = false;
};

#endif // MARC_FILE_NODE_H
