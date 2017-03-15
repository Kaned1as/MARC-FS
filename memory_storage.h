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

#ifndef MEMORY_STORAGE_H
#define MEMORY_STORAGE_H

#include <vector>

#include "abstract_storage.h"

/**
 * @brief The MemoryStorage class - storage implementation using std::vector
 *        as a backing store.
 *
 * While blazingly fast, this storage type resides directly in RAM and is
 * very unsuitable for large files.
 */
class MemoryStorage : public AbstractStorage
{
public:
    MemoryStorage();

    virtual void open() override;
    virtual bool empty() /*const*/ override;
    virtual size_t size() /*const*/ override;
    virtual int read(char *buf, size_t size, uint64_t offset) /*const*/ override;
    virtual int write(const char *buf, size_t size, uint64_t offset) override;
    virtual void append(const char *buf, size_t size) override;
    virtual const char *readFully() override;
    virtual void clear() override;
    virtual void truncate(off_t size) override;

private:
    std::vector<char> data;
};

#endif // MEMORY_STORAGE_H
