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

#ifndef CLOUDFILE_H
#define CLOUDFILE_H

#include <string>

namespace Json {
    class Value;
}

/**
 * @brief The CloudFile class representation of the structure returned by ls API call
 */
class CloudFile
{
public:
    enum Type {
        File,
        Directory
    };

    CloudFile();
    CloudFile(const Json::Value &val);

    Type getType() const;
    void setType(const Type &value);

    std::string getKind() const;
    void setKind(const std::string &value);

    std::string getHome() const;
    void setHome(const std::string &value);

    std::string getName() const;
    void setName(const std::string &value);

    std::string getHash() const;
    void setHash(const std::string &value);

    uint64_t getMtime() const;
    void setMtime(const uint64_t &value);

    uint64_t getSize() const;
    void setSize(const uint64_t &value);

    std::string getVirusScan() const;
    void setVirusScan(const std::string &value);

private:
    Type type;
    std::string kind;       // seems to be the same as type
    std::string home;       // full remote path on server (with fname)
    std::string name;       // name of file
    std::string hash;       // hash of file
    uint64_t mtime;         // modification time
    uint64_t size;          // size of file
    std::string virusScan;  // "pass" usually
};

#endif // CLOUDFILE_H
