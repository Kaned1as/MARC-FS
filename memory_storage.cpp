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

#include "memory_storage.h"

#include <algorithm>

using namespace std;

MemoryStorage::MemoryStorage()
{

}

void MemoryStorage::open()
{
    // nothing to initialize, no-op
}

bool MemoryStorage::empty() /*const*/
{
    return data.empty();
}

size_t MemoryStorage::size() /*const*/
{
    return data.size();
}

int MemoryStorage::read(char *buf, size_t size, uint64_t offset)
{
    auto len = data.size();
    if (offset > len)
        return 0; // requested bytes above the size

    if (offset + size > len) {
        // requested size is more than we have
        copy_n(&data[offset], len - offset, buf);
        return static_cast<int>(len - offset);
    }

    // normal operation
    copy_n(&data[offset], size, buf);
    return static_cast<int>(size);
}

int MemoryStorage::write(const char *buf, size_t size, uint64_t offset)
{
    if (offset + size > data.size()) {
        data.resize(offset + size);
    }

    copy_n(buf, size, &data[offset]);
    return static_cast<int>(size);
}

void MemoryStorage::append(const char *buf, size_t size)
{
    vector<char> tail(&buf[0], &buf[size]);
    data.insert(data.end(), tail.begin(), tail.end());
}

const char *MemoryStorage::readFully()
{
    return &data.front();
}

void MemoryStorage::clear()
{
    data.clear();
    data.shrink_to_fit();
}

void MemoryStorage::truncate(off_t size)
{
    data.resize(static_cast<uint64_t>(size));
}
