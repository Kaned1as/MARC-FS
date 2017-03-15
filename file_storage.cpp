#include <unistd.h> // not available on non-unix

#include <string>

#include "file_storage.h"
#include "mru_metadata.h"
#include "utils.h"

using namespace std;

extern MruData fsMetadata;

// definition
std::atomic_ulong FileStorage::counter;

FileStorage::FileStorage()
{

}

void FileStorage::open()
{
    // get unique file name
    string name = to_string(counter++);
    filename = fsMetadata.cacheDir + '/' + name;

    // open temporary file
    data.open(filename, ios::out | ios::in | ios::trunc | ios::binary);
    if (!data) {
        // should not happen - we checked this dir in main
        throw ios::failure("Can't open cache dir!");
    }
}


bool FileStorage::empty()
{
    if (!data.is_open())
        return true;

    data.seekg(0, ios::end);
    if (data.tellg()) {
        return false;
    }
    return true;
}

size_t FileStorage::size()
{
    if (!data.is_open())
        return 0;

    data.seekg(0, ios::end);
    return static_cast<size_t>(data.tellg());
}

int FileStorage::read(char *buf, size_t size, uint64_t offset)
{
    // stream offsets are signed, but we're safe since we count everything
    // from the beginning of a file
    data.seekg(static_cast<streamoff>(offset));
    data.read(buf, static_cast<streamoff>(size));
    if (data.eof()) // read may fail if we reach eof
        data.clear(); // clear possible failbit
    return static_cast<int>(data.gcount());
}

int FileStorage::write(const char *buf, size_t size, uint64_t offset)
{
    data.seekp(static_cast<streamoff>(offset));
    data.write(buf, static_cast<streamoff>(size));
    return static_cast<int>(data.tellp()) - static_cast<int>(offset);
}

void FileStorage::append(const char *buf, size_t size)
{
    data.seekp(0, ios::end);
    data.write(buf, static_cast<streamoff>(size));
}

const char *FileStorage::readFully()
{
    data.seekg(0);
    std::stringstream buffer;
    buffer << data.rdbuf();
    return buffer.str().data();
}

void FileStorage::clear()
{
    data.close();
    remove(filename.c_str());
}

void FileStorage::truncate(off_t size)
{
    // truncate is not cross-platform
    // TODO: check in C++17
    data.close();
    ::truncate(filename.c_str(), size);
    // don't truncate this time
    data.open(filename, ios::out | ios::in | ios::binary);
}
