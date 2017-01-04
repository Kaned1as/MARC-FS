#ifndef MRU_METADATA_H
#define MRU_METADATA_H

#include <map>
#include <vector>
#include "object_pool.h"
#include "marc_api.h"
#include "mru_node.h"

struct MruData {

    MruData() : clientPool(0) {

    }

    std::map<std::string, MruNode> cached;
    ObjectPool<MarcRestClient> clientPool;
};

#endif // MRU_METADATA_H
