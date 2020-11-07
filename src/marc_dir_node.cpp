/*****************************************************************************
 *
 * Copyright (c) 2018-2020, Oleg `Kanedias` Chernovskiy
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

#include "marc_dir_node.h"

MarcDirNode::MarcDirNode()
{

}

void MarcDirNode::fillStat(struct stat *stbuf) {
    MarcNode::fillStat(stbuf);

    stbuf->st_mode = S_IFDIR | 0700;
    stbuf->st_nlink = 2;
    stbuf->st_size = 0;

    stbuf->st_blocks = 1;

#ifndef __APPLE__
    stbuf->st_mtim.tv_sec = 0;
#else
    stbuf->st_mtimespec.tv_sec = 0;
#endif
}