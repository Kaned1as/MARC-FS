/*****************************************************************************
 *
 * Copyright (c) 2018-2019, Oleg `Kanedias` Chernovskiy
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

// man renameat2 - these constants are not present in glibc < 2.27
# define RENAME_NOREPLACE (1 << 0)
# define RENAME_EXCHANGE (1 << 1)
# define RENAME_WHITEOUT (1 << 2)

const static std::regex COMPOUND_REGEX("(.+)(\\.marcfs-part-)(\\d+)");
const static std::regex SHARE_LINK_REGEX("(.+)(\\.marcfs-link)(.*)");

ObjectPool<MarcRestClient> clientPool;
std::string cacheDir;

using namespace std;

static int doWithRetry(std::function<int(MarcRestClient *)> what) {
    uint retries = 3;

    while (retries > 0) {
        try {
            auto client = clientPool.acquire();
            return what(client.get());
        } catch (MailApiException &exc) {
            cerr << "Error in " << __FUNCTION__ << ": " << exc.what() << endl;
            if (exc.getResponseCode() >= 500) {
                retries--;
                continue;
            }

            return -EIO;
        }
    }

    // should never happen
    return -EINVAL;
}

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
static int handleLinks(string filePath, MarcFileNode* file) {
    smatch match;
    if (!regex_match(filePath, match, SHARE_LINK_REGEX)) {
        return 0;
    }

    string origPath = match[1];
    // string linkType = match[3];

    // get info of original file
    struct stat origFileInfo = {};
    getattrCallback(origPath.data(), &origFileInfo, nullptr);

    string link;
    if (origFileInfo.st_size > MARCFS_MAX_FILE_SIZE) {
        // it's compound, retrieve links for each part
        off_t partCount = (origFileInfo.st_size / MARCFS_MAX_FILE_SIZE) + 1;
        doWithRetry([&](MarcRestClient *client) {
            for (off_t idx = 0; idx < partCount; ++idx) {
                string extendedPath = origPath + MARCFS_SUFFIX + to_string(idx);
                link += extendedPath + ": ";
                link += SCLD_PUBLICLINK_ENDPOINT + '/' + client->share(extendedPath) + '\n';
            }
            return 0;
        });
    } else {
        // single file, single link
        doWithRetry([&](MarcRestClient *client) {
            link += origPath + ": ";
            link += SCLD_PUBLICLINK_ENDPOINT + '/' + client->share(origPath) + '\n';
            return 0;
        });
    }

    file->truncate(0);
    file->write(link.data(), link.size(), 0);

    return 0;
}

int utimensCallback(const char *path, const struct timespec time[2], struct fuse_file_info *fi)
{
    // API doesn't support utime, only seconds
    // not sure how to update this on remote fs, so update only if it's opened now

    if (fi && fi->fh) {
        // it's opened file, apply mtime
        auto file = reinterpret_cast<MarcFileNode *>(fi->fh);
        file->setMtime(time[1].tv_sec);
    }

    // not in opened, not in cache, skip the call
    return 0;
}

int chmodCallback(const char */*path*/, mode_t /*mode*/, fuse_file_info */*fi*/)
{
    // stub, no access rights for your own cloud
    return 0;
}

void * initCallback(fuse_conn_info *conn, fuse_config *cfg)
{
    conn->want |= FUSE_CAP_ASYNC_READ;
    conn->want |= FUSE_CAP_DONT_MASK;

    cfg->entry_timeout = 60;
    cfg->attr_timeout = 60;
    cfg->negative_timeout = 60;
    return nullptr;
}

int getattrCallback(const char *path, struct stat *stbuf, fuse_file_info *fi)
{
    // retrieve path to containing dir
    string pathStr(path); // e.g. /home/1517.svg

    if (pathStr == "/") { // special handling for root
        emptyStat(stbuf, S_IFDIR);
        return 0;
    }

    if (fi && fi->fh) {
        auto file = reinterpret_cast<MarcNode *>(fi->fh);
        file->fillStat(stbuf);
        return 0;
    }

    bool trailingSlash = pathStr[pathStr.size() - 1] == '/'; // true only for '/' dir
    auto slashPos = pathStr.find_last_of('/'); // that would be 5 for /home/1517.svg
    if (slashPos == string::npos)
        return -EIO;

    // get containing dir name and filename
    string dirname = pathStr.substr(0, slashPos); // that would be /home
    string filename = pathStr.substr(slashPos + 1); // that would be 1517.svg

    // try stat cache first
    auto statCache = CacheManager::getInstance();
    auto cached = statCache->get(pathStr);
    if (cached) {
        // have entry in cache, fill
        *stbuf = cached->stbuf;
        return 0;
    }

    // not found in cache, find requested file on cloud
    // get a listing of a containing dir for this file
    return doWithRetry([&](MarcRestClient *client) {
        bool found = false;
        string dirPath = dirname + (trailingSlash ? "" : "/"); // dir with slash at the end
        auto contents = client->ls(dirPath); // actual API call - ls the directory that contains this file

        // file may be compound one here
        // this may happen when calling by absolute path first (without readdir cache)
        handleCompounds(contents);

        for (CloudFile &cf : contents) {
            string fullPath = dirPath + cf.getName();

            // put all retrieved files in cache    
            struct stat temp = {};
            fillStat(&temp, &cf);
            statCache->put(fullPath, CacheNode(temp));

            if (cf.getName() == filename) {
                // file found
                fillStat(stbuf, &cf);
                found = true;
            }
        }

        if (found)
            return 0;

        return -ENOENT;
    });
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

    return doWithRetry([&](MarcRestClient *client) {
        auto info = client->df();
        stat->f_fsid = {}; // ignored
        stat->f_bsize = 4096; // a guess!
        stat->f_blocks = info.total / stat->f_bsize;
        stat->f_bfree = stat->f_blocks - info.used / stat->f_bsize;
        stat->f_bavail = stat->f_bfree;
        stat->f_namemax = 256;
        return 0;
    });

}

int openCallback(const char *path, struct fuse_file_info *fi)
{
    return doWithRetry([&](MarcRestClient *client) {
        auto file = new MarcFileNode;
        file->open(client, path);

        // no errors
        fi->fh = reinterpret_cast<uintptr_t>(file);
        return 0;
    });
}

int opendirCallback(const char *path, fuse_file_info *fi)
{
    fi->fh = reinterpret_cast<uintptr_t>(new MarcDirNode);
    return 0;
}

int readdirCallback(const char *path, void *dirhandle, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info */*fi*/, enum fuse_readdir_flags /*flags*/)
{
    filler(dirhandle, ".", nullptr, 0, (fuse_fill_dir_flags) 0);
    filler(dirhandle, "..", nullptr, 0, (fuse_fill_dir_flags) 0);

    string pathStr(path); // e.g. /directory or /

    return doWithRetry([&](MarcRestClient *client) {
        auto contents = client->ls(pathStr);
        handleCompounds(contents);
        bool trailingSlash = pathStr[pathStr.size() - 1] == '/'; // true only for '/' dir

        auto statCache = CacheManager::getInstance();
        for (const CloudFile &cf : contents) {
            string fullPath = pathStr + (trailingSlash ? "" : "/") + cf.getName();

            struct stat stbuf = {};
            fillStat(&stbuf, &cf);
            statCache->put(fullPath, CacheNode(stbuf));

            filler(dirhandle, cf.getName().data(), &stbuf, 0, (fuse_fill_dir_flags) 0);
        }

        return 0;
    });
}


int releasedirCallback(const char *path, fuse_file_info *fi)
{
    auto file = reinterpret_cast<MarcDirNode *>(fi->fh);
    delete file;
    return 0;
}

int readCallback(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    auto file = reinterpret_cast<MarcFileNode *>(fi->fh);
    auto offsetBytes = static_cast<uint64_t>(offset);
    return file->read(buf, size, offsetBytes);
}

int writeCallback(const char *path, const char *buf, size_t size, off_t offset, fuse_file_info *fi)
{
    auto file = reinterpret_cast<MarcFileNode *>(fi->fh);
    auto offsetBytes = static_cast<uint64_t>(offset);
    return file->write(buf, size, offsetBytes);
}


int flushCallback(const char *path, struct fuse_file_info *fi)
{
    auto file = reinterpret_cast<MarcFileNode *>(fi->fh);

    // handle possible link files
    int res = handleLinks(path, file);
    if (res)
        return res;

    return doWithRetry([&](MarcRestClient *client) {
        file->flush(client, path);
        CacheManager::getInstance()->update(path, *file);
        return 0;
    });
}

int releaseCallback(const char *path, struct fuse_file_info *fi)
{
    auto file = reinterpret_cast<MarcFileNode *>(fi->fh);
    file->release();
    delete file;

    return 0;
}

int mkdirCallback(const char *path, mode_t /*mode*/)
{
    return doWithRetry([&](MarcRestClient *client) {
        client->mkdir(path);
        return 0;
    });
}

int rmdirCallback(const char *path)
{
    return doWithRetry([&](MarcRestClient *client) {
        auto contents = client->ls(path);
        if (!contents.empty())
            return -ENOTEMPTY; // should we really? Cloud seems to be OK with it...

        client->remove(path);
        return 0;
    });
}

/**
 * Unlink is only for files, directories get @ref rmdir instead
 */
int unlinkCallback(const char *path)
{
    // should we check for the file first?
    struct stat stbuf = {};
    int res = getattrCallback(path, &stbuf, nullptr);
    if (res)
        return res;

    // IMPORTANT!
    // if a file is opened (e.g. flushed) by another thread and we call rm (unlink) on it, then FUSE
    // calls QUEUE PATH on this node and waits till the file is released
    // then calls DEQUEUE PATH on this node and calls:
    // 1.          rename file -> .fuse_hidden{...}
    //    e.g.     rename /test/VTS_03_2.VOB -> /test/.fuse_hidden000063dd00000001
    // 2. wait for release file .fuse_hidden{...}
    // 3.          unlink file .fuse_hidden{...}

    return doWithRetry([&](MarcRestClient *client) {
        MarcFileNode(stbuf).remove(client, path);
        return 0;
    });
}

int renameCallback(const char *oldPath, const char *newPath, unsigned int flags)
{
    struct stat oldStat = {};
    int srcErr = getattrCallback(oldPath, &oldStat, nullptr);
    if (srcErr)
        return srcErr;
    
    MarcFileNode sourceFile(oldStat);
    return doWithRetry([&](MarcRestClient *client) {
        // get info about the target
        struct stat newStat = {};
        int targetErr = getattrCallback(newPath, &newStat, nullptr);
        std::cout << "aff2s" << std::endl;

        if (targetErr == 0 /* target exists */) {
            MarcFileNode targetFile(newStat);

            if (flags & RENAME_NOREPLACE) {
                // file exists and replacement is not allowed
                return -EEXIST;
            }

            if (flags & RENAME_EXCHANGE) {
                // exchange source and target
                targetFile.rename(client, newPath, string(newPath) + ".marcfs-temp");
                sourceFile.rename(client, oldPath, newPath);
                targetFile.rename(client, string(newPath) + ".marcfs-temp", oldPath);
                return 0;
            }

            // it's not exchange and replace is permitted, delete the file
            targetFile.remove(client, newPath);
        }

        // if we write new file and try to rename it while flushing to cloud
        // it will fail here with -EIO as actual addition happens only after file is uploaded
        sourceFile.rename(client, oldPath, newPath);
        return 0;
    });
}

int truncateCallback(const char *path, off_t size, fuse_file_info *fi)
{
    if (fi && fi->fh) {
        // file is opened, truncate it
        auto file = reinterpret_cast<MarcFileNode *>(fi->fh);
        file->truncate(size);
        return 0;
    }

    // file is not open, just reupload it with requested size
    return doWithRetry([&](MarcRestClient *client) {
        // imitate reupload
        MarcFileNode tempFile;
        tempFile.open(client, path);
        tempFile.truncate(size);
        tempFile.flush(client, path);
        tempFile.release();
        return 0;
    });

}

int mknodCallback(const char *path, mode_t /*mode*/, dev_t /*dev*/)
{
    return doWithRetry([&](MarcRestClient *client) {
        client->create(path);
        return 0;
    });
}
