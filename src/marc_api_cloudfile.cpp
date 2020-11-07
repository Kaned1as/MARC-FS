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

#include "marc_api_cloudfile.h"

#include <json/value.h>
#include <sys/stat.h>

CloudFile::CloudFile()
{

}

CloudFile::CloudFile(const Json::Value &val) {
    type = val["type"].asString() == "file" ? S_IFREG : S_IFDIR;
    kind = val["kind"].asString();
    home = val["home"].asString();
    name = val["name"].asString();
    hash = val["hash"].asString();
    size = val["size"].asUInt64();
    mtime = val["mtime"].asInt64();
    virusScan = val["virus_scan"].asString();
}

int CloudFile::getType() const
{
    return type;
}

void CloudFile::setType(const int &value)
{
    type = value;
}

std::string CloudFile::getKind() const
{
    return kind;
}

void CloudFile::setKind(const std::string &value)
{
    kind = value;
}

std::string CloudFile::getHome() const
{
    return home;
}

void CloudFile::setHome(const std::string &value)
{
    home = value;
}

std::string CloudFile::getName() const
{
    return name;
}

void CloudFile::setName(const std::string &value)
{
    name = value;
}

std::string CloudFile::getHash() const
{
    return hash;
}

void CloudFile::setHash(const std::string &value)
{
    hash = value;
}

time_t CloudFile::getMtime() const
{
    return mtime;
}

void CloudFile::setMtime(const time_t &value)
{
    mtime = value;
}

uint64_t CloudFile::getSize() const
{
    return size;
}

void CloudFile::setSize(const uint64_t &value)
{
    size = value;
}

std::string CloudFile::getVirusScan() const
{
    return virusScan;
}

void CloudFile::setVirusScan(const std::string &value)
{
    virusScan = value;
}
