/*****************************************************************************
 *
 * Copyright (c) 2018, Oleg `Kanedias` Chernovskiy
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

#ifndef FILE_STORAGE_H
#define FILE_STORAGE_H

#include <atomic>
#include <fstream>
#include <mutex>
#include "abstract_storage.h"

/**
 * @brief The FileStorage class - storage implementation using file
 *        as a backing store.
 *
 * This storage type does not waste RAM but can cause delays as filesystem
 * access means IO on HDD/SSD.
 *
 */
class FileStorage : public AbstractStorage
{
public:
    FileStorage();

    virtual void open() override;
    virtual bool empty() override;
    virtual off_t size() override;
    virtual int read(char *buf, size_t size, uint64_t offset) override;
    virtual int write(const char *buf, size_t size, uint64_t offset) override;
    virtual void append(const char *buf, size_t size) override;
    virtual std::string readFully() override;
    virtual void clear() override;
    virtual void truncate(off_t size) override;
private:
    static std::atomic_ulong counter;

    std::mutex rw_mutex;
    std::string filename;
    std::basic_fstream<char> data;
};

#endif // FILE_STORAGE_H
