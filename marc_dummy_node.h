#ifndef MARC_DUMMY_NODE_H
#define MARC_DUMMY_NODE_H

#include "marc_node.h"

class MarcDummyNode : public MarcNode
{
public:

    MarcDummyNode();

    bool exists() const override;
};

#endif // MARC_DUMMY_NODE_H
