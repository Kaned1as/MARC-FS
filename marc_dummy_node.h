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

#ifndef MARC_DUMMY_NODE_H
#define MARC_DUMMY_NODE_H

#include "marc_node.h"

/**
 * @brief The MarcDummyNode class - used to indicate that file is non-existing.
 *        Implements negative cache for getattr lookups.
 */
class MarcDummyNode : public MarcNode
{
public:

    MarcDummyNode();

    bool exists() const override;
    void rename(MarcRestClient *client, std::string oldPath, std::string newPath) override;
};

#endif // MARC_DUMMY_NODE_H
