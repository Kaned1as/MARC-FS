#include "marc_node.h"

#include "utils.h"

MarcNode::MarcNode()
{

}

MarcNode::~MarcNode()
{

}

void MarcNode::setMtime(time_t time)
{
    this->mtime = time;
}

bool MarcNode::exists() const
{
    return true;
}

void MarcNode::fillStats(struct stat *stbuf) const
{
    if (!exists())
        throw std::invalid_argument("Tried to fill stats from non-existing file!");

    auto ctx = fuse_get_context();
    stbuf->st_uid = ctx->uid; // file is always ours, as long as we're authenticated
    stbuf->st_gid = ctx->gid;
    stbuf->st_mtim.tv_sec = mtime;
}

std::mutex& MarcNode::getMutex()
{
    return mutex;
}
