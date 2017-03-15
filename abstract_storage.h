#ifndef ABSTRACT_STORAGE_H
#define ABSTRACT_STORAGE_H

#include <cstdlib>
#include <cstdint>
#include <vector>
#include <type_traits>

/**
 * @brief The AbstractStorage class - where to save intermediary FUSE filesystem data
 *
 * This class serves as a base class for any storage type that can participate
 * in filesystem caching for Mail.ru Cloud FUSE operations.
 *
 * Both API and FUSE hooks are designed in a way to make use of it.
 *
 * @see MarcFileNode
 * @see MarcRestClient
 *
 */
class AbstractStorage
{
public:
    AbstractStorage();
    virtual ~AbstractStorage();

    /**
     * @brief open - initialize all needed underlying systems.
     * Operations that may continue - @ref read, @ref write,
     * @ref truncate, @ref append, @ref readFully, @ref truncate, @ref clear
     */
    virtual void open() = 0;

    /**
     * @brief empty - check if storage is empty
     * @return true if it's empty, false otherwise
     */
    virtual bool empty() = 0;

    /**
     * @brief size - return size of data currently in storage
     * @return size of underlying data in bytes
     */
    virtual size_t size() = 0;

    /**
     * @brief read - read from storage to buffer @ref buf
     * @param buf - buffer to feed data into
     * @param size - size to write to buffer
     * @param offset - starting read offset in the storage
     */
    virtual int read(char *buf, size_t size, uint64_t offset) = 0;

    /**
     * @brief write - write to storage from buffer @ref buf
     * @param buf - buffer to read data to write from
     * @param size - size to read from buffer
     * @param offset - starting write offset in the storage
     */
    virtual int write(const char *buf, size_t size, uint64_t offset) = 0;

    /**
     * @brief append - similar to write but appends data to the end of backing storage
     */
    virtual void append(const char *buf, size_t size) = 0;

    /**
     * @brief readFully - read data from storage fully and return it as byte buffer
     * @return pointer to retrieved data
     */
    virtual const char* readFully() = 0;

    /**
     * @brief clear - Clear all underlying systems and free associated resources.
     *                Next call can be only @ref open()
     */
    virtual void clear() = 0;

    /**
     * @brief truncate - resize the storage to have exactly this count of bytes
     * @param size - size of bytes to have. If it exceedes previous size, fill with 0-bytes
     */
    virtual void truncate(off_t size) = 0;
};

#endif // ABSTRACT_STORAGE_H
