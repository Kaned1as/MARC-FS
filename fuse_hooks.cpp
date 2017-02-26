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

#include <regex>
#include <unordered_map>

#include "fuse_hooks.h"

#include "marc_file_node.h"
#include "marc_dir_node.h"

#define API_CALL_TRY_BEGIN  \
    try { \
        auto client = fsMetadata.clientPool.acquire();

#define API_CALL_TRY_FINISH \
    } catch (MailApiException &exc) { \
        cerr << "Error in " << __FUNCTION__ << ": " << exc.what() << endl; \
        if (exc.getResponseCode() >= 500) \
            return -EAGAIN; \
        return -EIO;\
    }

const static std::regex COMPOUND_REGEX("(.+)(\\.marcfs-part-)(\\d+)");

using namespace std;

// explicit instantiation declarations
extern template LockHolder<MarcNode> MruData::getNode(string);
extern template LockHolder<MarcFileNode> MruData::getNode(string);
extern template LockHolder<MarcDirNode> MruData::getNode(string);
extern template void MruData::create<MarcDirNode>(string);
extern template void MruData::create<MarcFileNode>(string);

/**
 * @brief handleCompounds - collapse compounds into regular files with greater size
 * @param dirPath - path to containing dir
 * @param files - vector to mutate
 */
void handleCompounds(vector<CloudFile> &files) {
    unordered_map<string, CloudFile> compounds;
    auto newEnd = remove_if(files.begin(), files.end(), [&](CloudFile &file){
        string fileName = file.getName();

        // detect compounds
        std::smatch match;
        if (std::regex_match(fileName, match, COMPOUND_REGEX)) {
            string origName = match[1];

            if (compounds.find(origName) == compounds.end()) {
                auto complex = CloudFile(file);
                complex.setName(origName);
                compounds[origName] = complex;
            } else {
                CloudFile &complex = compounds[origName];
                complex.setSize(complex.getSize() + file.getSize());
            }
            // it was a compound file, remove it
            return true;
        }
        return false;
    });
    // erase compound parts from original iterator
    files.erase(newEnd, files.end());
    // populate with collapsed compounds
    for_each(compounds.cbegin(), compounds.cend(), [&](auto it){ files.push_back(it.second); });
}

int utimeCallback(const char */*path*/, utimbuf */*utime*/)
{
    // API doesn't support utime, only seconds
    // stub
    return 0;
}

int chmodCallback(const char */*path*/, mode_t /*mode*/)
{
    // stub, no access rights for your own cloud
    return 0;
}

void * initCallback(fuse_conn_info *conn)
{
#ifndef __APPLE__ // APPLE doesn't yet have this in osxfuse
    conn->want |= FUSE_CAP_BIG_WRITES; // writes more than 4096

    // confusing, right?
    // one would think we support async reads because we can invoke downloadAsync from API
    // but here's the thing - these download-related reads come *sequentially*,
    // with each next offset being equal to last + total.
    // FUSE_CAP_ASYNC_READ value means that multiple reads with different offsets
    // will come in one time. We surely can't support this.
    conn->want &= static_cast<unsigned>(~FUSE_CAP_ASYNC_READ);
    conn->async_read = 0;
#endif
    return nullptr;
}

int getattrCallback(const char *path, struct stat *stbuf)
{
    // retrieve path to containing dir
    string pathStr(path); // e.g. /home/1517.svg

    // try to retrieve it from cache
    auto node = fsMetadata.getNode<MarcNode>(pathStr);
    if (node) { // have this file in stat cache
        if (!node->exists()) // we cached that this file doesn't exist previously, just repeat
            return -ENOENT;

        // it exist, fill stat buffer
        node->fillStats(stbuf);
        return 0;
    }

    if (pathStr == "/") { // special handling for root
        fsMetadata.create<MarcDirNode>(pathStr);
        fsMetadata.getNode<MarcDirNode>(pathStr)->fillStats(stbuf);
        return 0;
    }

    // not found in cache, find requested file on cloud
    auto slashPos = pathStr.find_last_of('/'); // that would be 5 for /home/1517.svg
    if (slashPos == string::npos)
        return -EIO;

    // get containing dir name and filename
    string dirname = pathStr.substr(0, slashPos + 1); // that would be /home
    string filename = pathStr.substr(slashPos + 1); // that would be 1517.svg

    // API call required
    API_CALL_TRY_BEGIN
    auto contents = client->ls(dirname); // actual API call - ls the directory that contains this file

    // file may be compound one here
    // this may happen whan calling by absolute path first (without readdir cache)
    handleCompounds(contents);


    auto file = find_if(contents.cbegin(), contents.cend(), [&](const CloudFile &file) {
        return file.getName() == filename; // now find file in returned results
    });

    // file found - fill statbuf
    if (file != contents.cend()) {
        fsMetadata.putCacheStat(path, &(*file));
        fsMetadata.getNode<MarcNode>(path)->fillStats(stbuf);
        return 0;
    }
    API_CALL_TRY_FINISH


    // file not found - put negative mark in cache
    fsMetadata.putCacheStat(path, nullptr);
    return -ENOENT;
}

int statfsCallback(const char */*path*/, struct statvfs *stat)
{
    /**
     *   uint32 f_bsize; // optimal transfer block size
     *   uint32 f_blocks; // total block count in fs
     *   uint32 f_bfree; // free block count in fs
     *   uint32 f_bavail; // free block count available for non-root
     *   uint32 f_files; // total node count in filesystem
     *   uint32 f_ffree; // free node count in filesystem
     *   struct { int sid[2]; } f_fsid; // ids - major, minor of filesystem
     *   uint32 f_namelen; // maximum file naming length
     *   uint32 f_spare[6]; // reserved
     */

    API_CALL_TRY_BEGIN
    auto info = client->df();
    stat->f_fsid = {}; // ignored
    stat->f_bsize = 4096; // a guess!
    stat->f_blocks = info.totalMiB * 256; // * 1024 * 1024 / f_bsize
    stat->f_bfree = stat->f_blocks - info.usedMiB * 256;
    stat->f_bavail = stat->f_bfree;
    stat->f_namemax = 256;
    return 0;
    API_CALL_TRY_FINISH

}

int createCallback(const char *path, mode_t mode, fuse_file_info *fi)
{
    auto res = mknodCallback(path, mode, 0);
    if (res)
        return res;

    return openCallback(path, fi);
}

int openCallback(const char *path, struct fuse_file_info */*fi*/)
{
    // file should already be present as FUSE does `getattr` call prior to open
    auto file = fsMetadata.getNode<MarcFileNode>(path);

    API_CALL_TRY_BEGIN
    file->open(client.get(), path);
    API_CALL_TRY_FINISH

    return 0;
}

int readdirCallback(const char *path, void *dirhandle, fuse_fill_dir_t filler, off_t /*offset*/, struct fuse_file_info */*fi*/)
{
    filler(dirhandle, ".", nullptr, 0);
    filler(dirhandle, "..", nullptr, 0);

    string pathStr(path); // e.g. /directory or /

    // try cache first
    if (fsMetadata.tryFillDir(pathStr, dirhandle, filler))
        return 0;

    API_CALL_TRY_BEGIN
    auto contents = client->ls(pathStr);
    handleCompounds(contents);
    bool trailingSlash = pathStr[pathStr.size() - 1] == '/'; // true for '/' dir

    for (const CloudFile &cf : contents) {
        string fullPath = pathStr + (trailingSlash ? "" : "/") + cf.getName();

        struct stat stbuf = {};
        fsMetadata.putCacheStat(fullPath, &cf);
        fsMetadata.getNode<MarcNode>(fullPath)->fillStats(&stbuf);
        filler(dirhandle, cf.getName().data(), &stbuf, 0);
    }

    // confirm readdir cache
    auto node = fsMetadata.getNode<MarcDirNode>(pathStr);
    node->setCached(true);
    API_CALL_TRY_FINISH

    return 0;
}


//int readCallbackAsync(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info */*fi*/)
//{
//    auto offsetBytes = static_cast<uint64_t>(offset);
//    uint64_t readNow = 0;
//    auto callRead = [&](auto &pipe) {
//        while (!pipe.exhausted()) {
//            // get next chunk, possibly blocking
//            readNow += pipe.pop(buf + readNow, size - readNow); // actual transfer of bytes to the buffer
//            if (readNow == size) // reached limit
//                break;
//        }
//    };
    
//    // we're reading this file, try to pipe it from the cloud
//    if (offsetBytes == 0) { // that's a start
//        // set up async API transfer that will fill our queue
//        // api object will return to the pool once download is complete
//        auto &pipe = fsMetadata.createTransfer(path);
//        auto handle = [&]() {
//            auto api = fsMetadata.clientPool.acquire();
//            auto file = fsMetadata.getOrCreateFile(path);
//            api->downloadAsync(path, pipe);
//        };
//        thread(handle).detach();

//        callRead(pipe);
//    } else {
//        auto &pipe = fsMetadata.getTransfer(path);
//        if (offsetBytes == pipe.getTransferred()) { // that's continuation of previous call
//        // transfer must already been set, continue
//            callRead(pipe);
//        } else {
//            // attempt to read a file not sequentially, abort
//            return -EBADE;
//        }
//    }

//    return static_cast<int>(readNow);
//}

// not working, see header file
//int writeCallbackAsync(const char *path, const char *buf, size_t size, off_t offset,struct fuse_file_info */*fi*/)
//{
//    auto offsetBytes = static_cast<uint64_t>(offset);
//    auto &file = fsMetadata.obtainFile(path);

//    auto writtenAlready = file->getTransferred();
//    vector<char> vec(buf, buf + size);
//    // we're writing this file, try to send it chunked to the cloud
//    if (offsetBytes == 0) { // that's a start
//        auto api = fsMetadata.clientPool.acquire();
//        // set up async API transfer that will fill our queue
//        auto handle = bind(&MarcRestClient::uploadAsync, api.get(), path, ref(file->getTransfer()));
//        thread(handle).detach();

//        file->getTransfer().push(vec); // actual transfer of bytes to the queue
//        file->setTransferred(writtenAlready + size);
//    } else if (offsetBytes == writtenAlready) { // that's continuation of previous call
//        // transfer must already been set, continue
//        file->getTransfer().push(vec);
//    } else {
//        // attempt to write a file not sequentially, abort
//        return -EBADE;
//    }

//    file->setDirty(true);
//    file->setTransferred(writtenAlready + size);
//    return static_cast<int>(size);
//}

int readCallback(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info */*fi*/)
{
    auto offsetBytes = static_cast<uint64_t>(offset);
    auto file = fsMetadata.getNode<MarcFileNode>(path);
    return file->read(buf, size, offsetBytes);
}

int writeCallback(const char *path, const char *buf, size_t size, off_t offset, fuse_file_info */*fi*/)
{
    auto offsetBytes = static_cast<uint64_t>(offset);
    auto file = fsMetadata.getNode<MarcFileNode>(path);
    return file->write(buf, size, offsetBytes);
}


int flushCallback(const char *path, struct fuse_file_info */*fi*/)
{
    auto file = fsMetadata.getNode<MarcFileNode>(path); // present as we opened it earlier

    API_CALL_TRY_BEGIN
    file->flush(client.get(), path);
    API_CALL_TRY_FINISH

    return 0;
}

int releaseCallback(const char *path, struct fuse_file_info */*fi*/)
{
    auto file = fsMetadata.getNode<MarcFileNode>(path);
    auto &vec = file->getCachedContent();
    file->setSize(vec.size()); // set cached size to last content size before clearing
    vec.clear(); // forget contents of a node
    vec.shrink_to_fit();

    return 0;
}

int mkdirCallback(const char *path, mode_t /*mode*/)
{
    API_CALL_TRY_BEGIN
    client->mkdir(path);
    fsMetadata.create<MarcDirNode>(path);
    API_CALL_TRY_FINISH

    return 0;
}

int rmdirCallback(const char *path)
{
    API_CALL_TRY_BEGIN
    auto contents = client->ls(path);
    if (!contents.empty())
        return -ENOTEMPTY; // should we really? Cloud seems to be OK with it...

    client->remove(path);
    fsMetadata.purgeCache(path);
    API_CALL_TRY_FINISH

    return 0;
}

int unlinkCallback(const char *path)
{
    API_CALL_TRY_BEGIN
    {
        auto file = fsMetadata.getNode<MarcFileNode>(path);
        file->remove(client.get(), path);
        // -- unlock --
    }
    fsMetadata.purgeCache(path);
    API_CALL_TRY_FINISH

    return 0;
}

int renameCallback(const char *oldPath, const char *newPath)
{
    API_CALL_TRY_BEGIN
    client->rename(oldPath, newPath);
    // FIXME: readdir cache will be corrupted!
    // TODO: just move in cache too
    fsMetadata.purgeCache(oldPath);
    fsMetadata.purgeCache(newPath);
    API_CALL_TRY_FINISH

    return 0;
}

int truncateCallback(const char *path, off_t size)
{
    auto file = fsMetadata.getNode<MarcFileNode>(path);

    auto &vec = file->getCachedContent();
    vec.resize(static_cast<uint64_t>(size));
    file->setDirty(true);
    return 0;
}

int mknodCallback(const char *path, mode_t /*mode*/, dev_t /*dev*/)
{
    // just mark it created in cache
    fsMetadata.create<MarcFileNode>(path);
    fsMetadata.getNode<MarcFileNode>(path)->setNewlyCreated(true);

    return 0;
}
