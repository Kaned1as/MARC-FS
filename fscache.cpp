#include "fscache.h"

#include <vector>
#include <cassert>


template <class K, class V>
FsCache<K, V>::~FsCache()
{

}

template<class K, class V>
void FsCache<K, V>::insert(const FsCache::KeyType &k, const FsCache::ValueType &v) {
    assert(keyToValue.find(k) == keyToValue.end()); // Method is only called on cache misses

    if (keyToValue.size() == capacity) // Make space if necessary
        evict();

    // Record k as most-recently-used key
    auto it = keyTracker.insert(keyTracker.end(), k);

    // Create the key-value entry,
    // linked to the usage record.
    keyToValue.insert(std::make_pair(k, std::make_pair(v, it)));
    // No need to check return,
    // given previous assert.
}

template<class K, class V>
void FsCache<K, V>::evict() {
    assert(!keyTracker.empty()); // Assert method is never called when cache is empty
    auto it = keyToValue.find(keyTracker.front()); // Identify least recently used key
    assert(it != keyToValue.end());
    keyToValue.erase(it); // Erase both elements to completely purge record
    keyTracker.pop_front();
}

// explicit instantiation part
template class FsCache<std::string, std::vector<int8_t>>;
