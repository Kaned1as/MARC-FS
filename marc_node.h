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

#ifndef MARC_NODE_H
#define MARC_NODE_H

#include <mutex>

#include "marc_api_cloudfile.h"

class MarcRestClient;

/**
 * @brief The MarcNode class - base cache node for any filesystem entry
 */
class MarcNode
{
public:
    MarcNode();
    virtual ~MarcNode();

    void setMtime(time_t time);

    virtual bool exists() const;

    /**
     * @brief fillStats - fills stat struct with info about the file.
     *        Use if exists() call returns `true`
     * @param stbuf - struct stat
     */
    virtual void fillStats(struct stat *stbuf) const;

    /**
     * @brief rename - rename file from old path to new. This operation
     *        may not be atomic for some file types.
     */
    virtual void rename(MarcRestClient *client, std::string oldPath, std::string newPath);

    std::mutex& getMutex();
};

#endif // MARC_NODE_H
