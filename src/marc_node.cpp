/*****************************************************************************
 *
 * Copyright (c) 2018-2019, Oleg `Kanedias` Chernovskiy
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

#include <fuse3/fuse.h>

#include "marc_rest_client.h"
#include "marc_node.h"
#include "utils.h"


using namespace std;

MarcNode::MarcNode()
{

}

MarcNode::~MarcNode()
{

}

void MarcNode::fillStat(struct stat *stbuf) 
{
    auto ctx = fuse_get_context();
    stbuf->st_uid = ctx->uid; // file is always ours, as long as we're authenticated
    stbuf->st_gid = ctx->gid;

    stbuf->st_blksize = 4096;
}

void MarcNode::rename(MarcRestClient *client, string oldPath, string newPath)
{
    // default implementation
    client->rename(oldPath, newPath);
}
