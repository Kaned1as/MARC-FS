/*****************************************************************************
 *
 * Copyright (c) 2017, Oleg `Kanedias` Chernovskiy
 *
 * This file is part of MARC-FS.
 *
 * MARC-FS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MARC-FS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MARC-FS.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SHARD_H
#define SHARD_H

#include <string>

namespace Json {
    class Value;
}

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

    static std::string asString(ShardType type) {
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
    const int& getCount() const;

private:
    const std::string url;
    const int count;
};

#endif // SHARD_H
