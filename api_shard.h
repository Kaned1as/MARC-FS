#ifndef SHARD_H
#define SHARD_H

#include <string>
#include <json/json.h>

class Shard
{
public:
    enum class ShardType : int {
        VIDEO = 0,
        VIEW_DIRECT,
        WEBLINK_VIEW,
        WEBLINK_VIDEO,
        WEBLINK_GET,
        WEBLINK_THUMBNAILS,
        AUTH,
        VIEW,
        GET,
        UPLOAD,
        THUMBNAILS
    };

    static std::string as_string(ShardType type) {
        switch (type) {
            case ShardType::VIDEO: return "video";
            case ShardType::VIEW_DIRECT: return "view_direct";
            case ShardType::WEBLINK_VIEW: return "weblink_view";
            case ShardType::WEBLINK_VIDEO: return "weblink_video";
            case ShardType::WEBLINK_GET: return "weblink_get";
            case ShardType::WEBLINK_THUMBNAILS: return "weblink_thumbnails";
            case ShardType::AUTH: return "auth";
            case ShardType::VIEW: return "view";
            case ShardType::GET: return "get";
            case ShardType::UPLOAD: return "upload";
            case ShardType::THUMBNAILS: return "thumbnails";
        }
    }

    Shard(const Json::Value &shardEntity);

    const std::string& getUrl() const;
    const uint32_t& getCount() const;

private:
    const std::string url;
    const uint32_t count;
};

#endif // SHARD_H
