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

#include "marc_dummy_node.h"

MarcDummyNode::MarcDummyNode()
{

}

bool MarcDummyNode::exists() const
{
    return false;
}

void MarcDummyNode::rename(MarcRestClient *client, std::string oldPath, std::string newPath)
{
    // we can't rename non-existing node, this file does not exist!
    throw std::invalid_argument("Tried to move non-existing file!");
}
