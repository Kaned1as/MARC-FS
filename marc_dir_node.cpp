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

#include "marc_dir_node.h"

#include <fuse.h>

MarcDirNode::MarcDirNode()
{

}

void MarcDirNode::fillStats(struct stat *stbuf) const
{
    // Mail.ru Cloud doesn't have mtime for dirs, sadly
    // so expect all mtimes of folders to be Jan 1 1970
    MarcNode::fillStats(stbuf);
    stbuf->st_mode = S_IFDIR | 0700;
    stbuf->st_nlink = 2;
}
