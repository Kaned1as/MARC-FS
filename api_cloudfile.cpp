#include "api_cloudfile.h"

CloudFile::CloudFile()
{

}

std::string CloudFile::getPath() const
{
    return path;
}

void CloudFile::setPath(const std::string &value)
{
    path = value;
}

std::string CloudFile::getName() const
{
    return name;
}

void CloudFile::setName(const std::string &value)
{
    name = value;
}

uint64_t CloudFile::getSize() const
{
    return size;
}

void CloudFile::setSize(const uint64_t &value)
{
    size = value;
}

CloudFile::Type CloudFile::getType() const
{
    return type;
}

void CloudFile::setType(const Type &value)
{
    type = value;
}
