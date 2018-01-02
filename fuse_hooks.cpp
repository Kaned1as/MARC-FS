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
const static std::regex SHARE_LINK_REGEX("(.+)(\\.marcfs-link)(.*)");

// global MruData instance definition
MruData fsMetadata;

using namespace std;

// explicit instantiation declarations
extern template MarcNode* MruData::getNode(string);
extern template MarcFileNode* MruData::getNode(string);
extern template MarcDirNode* MruData::getNode(string);
extern template void MruData::create<MarcDirNode>(string);
extern template void MruData::create<MarcFileNode>(string);

/**
 * @brief handleCompounds - collapse compounds into regular files with greater size
 *
 * Handling compounds is an important feature of this OS. If a file to be uploaded Mail.ru Cloud
 * is bigger than 2GB (which cloud don't support), then it's split to parts 1, 2, 3 etc.
 * all 2GB long.
 *
 * When such file is downloaded again, all these split files are joined back into one.
 *
 * Sample:
 *  name.mkv 3GB long ---> cloud
 *                      -> name.mkv.marcfs-part-1 2GB long
 *                      -> name.mkv.marcfs-part-2 1GB long
 *              local <---
 *  name.mkv 3GB long <-
 *
 * @param dirPath - path to containing dir
 * @param files - vector to mutate
 */
void handleCompounds(vector<CloudFile> &files) {
    unordered_map<string, CloudFile> compounds;
    auto newEnd = remove_if(files.begin(), files.end(), [&](CloudFile &file){
        string fileName = file.getName();

        // detect compounds
        smatch match;
        if (regex_match(fileName, match, COMPOUND_REGEX)) {
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

/**
 * @brief handleLinks - populate link files with content they need
 *
 * "Link files" is kind of special file needed for Mail.ru Cloud interaction.
 * In cloud, owner can share a file by obtaining public link to it and sending it
 * to whoever he wants. To support this feature on the filesystem level user
 * should create special file with same name as file to be shared but with
 * ".marcfs-link" ending appended. The read operation for such  file will
 * return the shared link url to that file.
 *
 * @param filePath - path to the link file
 * @param file - cached link file node
 */
int handleLinks(string filePath, MarcFileNode* file) {
    smatch match;
    if (!regex_match(filePath, match, SHARE_LINK_REGEX)) {
        return 0;
    }

    string origPath = match[1];
    // string linkType = match[3];

    auto origFile = fsMetadata.getNode<MarcFileNode>(origPath);
    string link;
    if (origFile->getSize() > MARCFS_MAX_FILE_SIZE) {
        // it's compound, retrieve links for each part
        size_t partCount = (origFile->getSize() / MARCFS_MAX_FILE_SIZE) + 1;
        API_CALL_TRY_BEGIN
        for (size_t idx = 0; idx < partCount; ++idx) {
            string extendedPath = origPath + MARCFS_SUFFIX + to_string(idx);
            link += extendedPath + ": ";
            link += SCLD_PUBLICLINK_ENDPOINT + '/' + client->share(extendedPath) + '\n';
        }
        API_CALL_TRY_FINISH
    } else {
        // single file, single link
        API_CALL_TRY_BEGIN
        link += origPath + ": ";
        link += SCLD_PUBLICLINK_ENDPOINT + '/' + client->share(origPath) + '\n';
        API_CALL_TRY_FINISH
    }
    file->truncate(0);
    file->write(link.data(), link.size(), 0);

    return 0;
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

        // it exists, fill stat buffer
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
    string dirname = pathStr.substr(0, slashPos + 1); // that would be /home/
    string filename = pathStr.substr(slashPos + 1); // that would be 1517.svg

    // API call required, try to find file on remote side
    bool found = false;
    API_CALL_TRY_BEGIN
    auto contents = client->ls(dirname); // actual API call - ls the directory that contains this file

    // file may be compound one here
    // this may happen when calling by absolute path first (without readdir cache)
    handleCompounds(contents);

    // put all retrieved files in cache
    for (CloudFile &file : contents) {
        fsMetadata.putCacheStat(dirname + file.getName(), &file);
        if (file.getName() == filename) {
            // file found - fill statbuf
            fsMetadata.getNode<MarcNode>(pathStr)->fillStats(stbuf);
            found = true;
        }
    }
    API_CALL_TRY_FINISH

    if (found)
        return 0;

    // not found - put negative mark in cache
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
    API_CALL_TRY_FINISH

    return 0;
}

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

    // handle possible link files
    int res = handleLinks(path, file);
    if (res)
        return res;

    API_CALL_TRY_BEGIN
    file->flush(client.get(), path);
    API_CALL_TRY_FINISH

    return 0;
}

int releaseCallback(const char *path, struct fuse_file_info */*fi*/)
{
    auto file = fsMetadata.getNode<MarcFileNode>(path);
    file->release();

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
    return fsMetadata.purgeCache(path);
    API_CALL_TRY_FINISH
}

/**
 * Unlink is only for files, directories get @ref rmdir instead
 */
int unlinkCallback(const char *path)
{
    // IMPORTANT!
    // if a file is opened (e.g. flushed) by another thread and we call rm (unlink) on it, then FUSE
    // calls QUEUE PATH on this node and waits till the file is released
    // then calls DEQUEUE PATH on this node and calls:
    // 1.          rename file -> .fuse_hidden{...}
    //    e.g.     rename /test/VTS_03_2.VOB -> /test/.fuse_hidden000063dd00000001
    // 2. wait for release file .fuse_hidden{...}
    // 3.          unlink file .fuse_hidden{...}

    API_CALL_TRY_BEGIN
    auto file = fsMetadata.getNode<MarcFileNode>(path);
    file->remove(client.get(), path);
    return fsMetadata.purgeCache(path);
    API_CALL_TRY_FINISH
}

int renameCallback(const char *oldPath, const char *newPath)
{
    auto node = fsMetadata.getNode<MarcNode>(oldPath);
    if (!node || !node->exists()) {
        return -ENOENT;
    }

    API_CALL_TRY_BEGIN
    auto target = fsMetadata.getNode<MarcNode>(newPath);
    auto targetFile = dynamic_cast<MarcFileNode*>(target);
    if (targetFile) {
        // we have file here, remove it
        targetFile->remove(client.get(), newPath);
    }

    // if we write new file and try to rename it while flushing to cloud
    // it will fail here with -EIO as actual addition happens only after file is uploaded
    node->rename(client.get(), oldPath, newPath);
    fsMetadata.renameCache(oldPath, newPath);
    API_CALL_TRY_FINISH

    return 0;
}

int truncateCallback(const char *path, off_t size)
{
    auto file = fsMetadata.getNode<MarcFileNode>(path);
    file->truncate(size);
    return 0;
}

int mknodCallback(const char *path, mode_t /*mode*/, dev_t /*dev*/)
{
    API_CALL_TRY_BEGIN
    client->create(path);
    fsMetadata.create<MarcFileNode>(path);
    fsMetadata.getNode<MarcFileNode>(path)->setNewlyCreated(true);
    API_CALL_TRY_FINISH

    return 0;
}
