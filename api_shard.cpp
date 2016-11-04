#include "api_shard.h"

Shard::Shard(Json::Value const &shardEntity)
    : url(shardEntity["url"].asString()),
      count(shardEntity["count"].asUInt())
{
}

const std::string& Shard::getUrl() const
{
    return url;
}

const uint32_t& Shard::getCount() const
{
    return count;
}
