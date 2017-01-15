#ifndef MARC_DIR_NODE_H
#define MARC_DIR_NODE_H

#include "marc_node.h"

class MarcDirNode : public MarcNode
{
public:
    MarcDirNode();

    void fillStats(struct stat *stbuf) const override;

    void setCached(bool value);
    bool isCached() const;

private:
    /**
     * whether or not readdir was invoked on that dir
     */
    bool contentsCached = false;
};

#endif // MARC_DIR_NODE_H
