#ifndef MRU_METADATA_H
#define MRU_METADATA_H

#include <map>
#include <vector>
#include "object_pool.h"
#include "api.h"
#include "mru_node.h"

struct MruData {

    MruData() : apiPool(1) {

    }

    std::map<std::string, MruNode> cached;
    ObjectPool<API> apiPool;
};

#endif // MRU_METADATA_H
