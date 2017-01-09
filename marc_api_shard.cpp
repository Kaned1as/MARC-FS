#include "marc_api_shard.h"

#include <json/value.h>

Shard::Shard(Json::Value const &shardEntity)
    : url(shardEntity[0]["url"].asString()),
      count(std::stoi(shardEntity[0]["count"].asString()))
{
}

const std::string& Shard::getUrl() const
{
    return url;
}

const int& Shard::getCount() const
{
    return count;
}
