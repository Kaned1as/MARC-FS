#include "marc_dir_node.h"

MarcDirNode::MarcDirNode()
{

}

void MarcDirNode::fillStats(struct stat *stbuf) const
{
    MarcNode::fillStats(stbuf);
    stbuf->st_mode = S_IFDIR | 0700;
    stbuf->st_nlink = 2;
}
