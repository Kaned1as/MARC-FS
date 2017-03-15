#ifndef FILE_STORAGE_H
#define FILE_STORAGE_H

#include <atomic>
#include <fstream>
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
    virtual size_t size() override;
    virtual int read(char *buf, size_t size, uint64_t offset) override;
    virtual int write(const char *buf, size_t size, uint64_t offset) override;
    virtual void append(const char *buf, size_t size) override;
    virtual const char *readFully() override;
    virtual void clear() override;
    virtual void truncate(off_t size) override;
private:
    static std::atomic_ulong counter;
    std::string filename;
    std::basic_fstream<char> data;
};

#endif // FILE_STORAGE_H
