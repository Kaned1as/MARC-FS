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

#ifndef MARC_FILE_NODE_H
#define MARC_FILE_NODE_H

#include <vector>
#include <memory>

#include "marc_node.h"
#include "abstract_storage.h"

class MarcRestClient;

/**
 * @brief The MarcFileNode class - cached node of regular file on the cloud
 */
class MarcFileNode : public MarcNode
{
public:
    MarcFileNode();

    virtual void fillStats(struct stat *stbuf) const override;
    void open(MarcRestClient *client, std::string path);
    void flush(MarcRestClient *client, std::string path);
    int read(char *buf, size_t size, uint64_t offsetBytes);
    int write(const char *buf, size_t size, uint64_t offsetBytes);
    void remove(MarcRestClient *client, std::string path);
    void truncate(off_t size);

    bool isDirty() const;
    void setDirty(bool value);

    void setSize(size_t fileSize);

    AbstractStorage& getCachedContent();

    bool isNewlyCreated() const;
    void setNewlyCreated(bool value);

private:
    size_t getSize() const;
    /**
     * @brief cachedContent - backing storage for open-write/read-release sequence
     */
    std::unique_ptr<AbstractStorage> cachedContent;
    /**
     * @brief dirty - used to indicate whether subsequent upload is needed
     */
    bool dirty = false;
    bool newlyCreated = false;
    /**
     * @brief fileSize - holds size that was passed from cloud (or sum of compounds)
     */
    size_t fileSize = 0;
};

#endif // MARC_FILE_NODE_H
