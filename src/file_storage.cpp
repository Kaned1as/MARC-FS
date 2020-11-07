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

#include <filesystem>
#include <unistd.h> // not available on non-unix

#include <string>
#include <cstring>

#include "file_storage.h"
#include "mru_cache.h"
#include "utils.h"

extern std::string cacheDir;

// definition
std::atomic_ulong FileStorage::counter;

FileStorage::FileStorage() {

}

void FileStorage::open()
{
    // get unique file name
    std::string name = std::to_string(counter++);
    filename = cacheDir + '/' + name;

    // open temporary file
    data.open(filename, std::ios::out | std::ios::in | std::ios::trunc | std::ios::binary);
    if (!data) {
        // should not happen - we checked this dir in main
        throw std::ios::failure("Can't open cache dir!");
    }
}


bool FileStorage::empty()
{
    if (!data.is_open())
        return true;

    data.seekg(0, std::ios::end);
    if (data.tellg()) {
        return false;
    }
    return true;
}

size_t FileStorage::size()
{
    if (!data.is_open())
        return 0;

    data.seekg(0, std::ios::end);
    return data.tellg();
}

int FileStorage::read(char *buf, size_t size, uint64_t offset)
{
    // concurrent access would destroy seek/tell sequence
    std::lock_guard<std::mutex> guard(rw_mutex);

    // stream offsets are signed, but we're safe since we count everything
    // from the beginning of a file
    data.seekg(static_cast<std::streamoff>(offset));
    data.read(buf, static_cast<std::streamoff>(size));
    if (data.eof()) // read may fail if we reach eof
        data.clear(); // clear possible failbit
    return static_cast<int>(data.gcount());
}

int FileStorage::write(const char *buf, size_t size, uint64_t offset)
{
    // concurrent access would destroy seek/tell sequence
    std::lock_guard<std::mutex> guard(rw_mutex);

    data.seekp(static_cast<std::streamoff>(offset));
    data.write(buf, static_cast<std::streamoff>(size));
    return static_cast<int>(data.tellp()) - static_cast<int>(offset);
}

void FileStorage::append(const char *buf, size_t size)
{
    data.seekp(0, std::ios::end);
    data.write(buf, static_cast<std::streamoff>(size));
}

std::string FileStorage::readFully()
{
    data.seekg(0);
    std::stringstream buffer;
    buffer << data.rdbuf();
    return buffer.str();
}

void FileStorage::clear()
{
    data.close();
    remove(filename.c_str());
}

void FileStorage::truncate(off_t size)
{
    data.close();
    std::filesystem::resize_file(filename, size);
    data.open(filename, std::ios::out | std::ios::in | std::ios::binary);
}
