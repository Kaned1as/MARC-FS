#ifndef MARC_DIR_NODE_H
#define MARC_DIR_NODE_H

#include "marc_node.h"

class MarcDirNode : public MarcNode
{
public:
    MarcDirNode();

    void fillStats(struct stat *stbuf) const override;
};

#endif // MARC_DIR_NODE_H
