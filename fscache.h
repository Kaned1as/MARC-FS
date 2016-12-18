#ifndef FSCACHE_H
#define FSCACHE_H

#include <list>
#include <map>
#include <functional>

template <class K, class V>
class FsCache
{
public:
    static FsCache& instance() {
        // lazy and thread-safe
        static FsCache s(30, 50L * 1024 * 1024 * 1024);
        return s;
    }

    using KeyType = K;
    using ValueType = V;

    // Key access history, most recent at back
    using KeyTrackerType = std::list<KeyType>;

    // Key to value and key history iterator
    using KeyToValueType = std::map<KeyType, std::pair<ValueType, class KeyTrackerType::iterator>>;

    // Obtain value of the cached function for k
    ValueType get(const KeyType& k) {
        // Attempt to find existing record
        const typename KeyToValueType::iterator it = keyToValue.find(k);
        if (it == keyToValue.end()) { // We don't have it:
            const ValueType v = cacheFunc(k); // Evaluate function and create new record
            insert(k, v);
            return v; // Return the freshly computed value
        } else { // We do have it:
            // Update access record by moving
            // accessed key to back of list
            keyTracker.splice(keyTracker.end(), keyTracker, (*it).second.second);
            return (*it).second.first; // Return the retrieved value
        }
    }

    // Obtain the cached keys, most recently used element
    // at head, least recently used at tail.
    // This method is provided purely to support testing.
    template <typename IT> void getKeys(IT dst) const {
        auto src = keyTracker.rbegin();
        while (src != keyTracker.rend()) {
            *dst++ = *src++;
        }
    }

    void init(ValueType (*function)(const KeyType& k)) {
        cacheFunc = function;
    }

private:
    FsCache(std::size_t cap, uint64_t maxMem)
        : capacity(cap), maxMemory(maxMem)
    {
    };

    ~FsCache();

    // disable copy
    FsCache(FsCache const&) = delete;
    FsCache& operator =(FsCache const&) = delete;

    // Record a fresh key-value pair in the cache
    void insert(const KeyType& k, const ValueType& v);

    // Purge the least-recently-used element in the cache
    void evict();

    // The function to be cached
    std::function<ValueType(const KeyType& k)> cacheFunc;

    // Maximum number of key-value pairs to be retained
    const std::size_t capacity;
    // Maxumum memory size of all values
    const uint64_t maxMemory;

    // Key access history
    KeyTrackerType keyTracker;

    // Key-to-value lookup
    KeyToValueType keyToValue;
};

#endif // FSCACHE_H
