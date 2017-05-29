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

#ifndef API_H
#define API_H

#include "account.h"
#include "marc_api_shard.h"
#include "marc_api_cloudfile.h"

#include "curl_cookie.h"

#include "utils.h"

#define MARCFS_MAX_FILE_SIZE ((1UL << 31) - (1UL << 10)) // 2 GB except 1 KB for multipart boundaries etc.
//#define MARCFS_MAX_FILE_SIZE (1L << 25) // 32 MiB - for tests
#define MARCFS_SUFFIX ".marcfs-part-"

extern const std::string SCLD_PUBLICLINK_ENDPOINT;

/**
 * @brief The MarcRestClient class - Abstraction layer between FUSE API and Mail.ru Cloud API.
 *
 * This class is used in an object pool to perform various lookups/calls to Mail.ru cloud.
 * It operates via cURL calls to specified endpoints. There is no public Cloud docs exposed
 * so all calls here are reverse-engineered from browser-to-cloud interaction.
 *
 * cURL connects via HTTPS protocol, so uses libcrypto PKIX. This means that there are
 * engine initialization, handshake, keys and other memory-pressing entities created
 * as a result of exchange. Valgrind may report false-positives for calls from here
 * that appear because libcrypto initializes its engine partially on garbage heap data.
 *
 * @see mru_metadata.h
 * @see fuse_hooks.cpp
 */
class MarcRestClient
{
public:
    using Params = std::map<std::string, std::string>;

    MarcRestClient();

    /**
     * @brief MarcRestClient - for copying already authenticated rest client.
     *                         copies account and cookies from the argument
     * @param toCopy - readied client
     */
    MarcRestClient(MarcRestClient &toCopy);

    /**
     * @brief setProxy - set proxy URL to use. Syntax is same as in libcurl API.
     * @param proxyUrl - string representing proxy URL (better with scheme). Should not be empty.
     */
    void setProxy(std::string proxyUrl);

    /**
     * @brief API::login Sends auth info and initializes this API object on successful login.
     * @param acc account to auth with
     * @return true if authenticated successfully, false otherwise.
     */
    bool login(const Account& acc);

    /**
     * @brief upload uploads bytes in @param body to remote endpoint
     * @param remotePath remote path to folder where uploaded file should be (e.g. /newfolder)
     */
    template<typename Container>
    void upload(std::string remotePath, Container &body, size_t start = 0, size_t count = SIZE_MAX);

    /**
     * @brief create - create empty file at path
     */
    void create(std::string remotePath);

    /**
     * @brief mkdir creates a directory noted by remotePath
     * @param remote_path new directory absolute path (e.g. /home/newfolder)
     */
    void mkdir(std::string remotePath);

    /**
     * @brief ls list entries in a directory
     * @param remote_path absolute path to directory to list
     * @return vector of file items
     */
    std::vector<CloudFile> ls(std::string remotePath);

    /**
     * @brief download download file pointed by remotePath to local path
     * @param remotePath remote path on cloud server
     * @param target target of download operation - resulting bytes are appended there
     */
    template<typename Container>
    void download(std::string remotePath, Container &target);

    /**
     * @brief remove removes file pointed by remotePath from cloud storage
     * @param remotePath remote path on cloud server
     */
    void remove(std::string remotePath);

    /**
     * @brief df - report file system disk space usage
     *
     * REST API actually doesn't provide exact numbers of total bytes,
     * block size, used blocks etc., so only rough precision may be used.
     *
     * @return SpaceInfo struct with total and used fields filled in
     */
    SpaceInfo df();

    /**
     * @brief rename - renames (moves) file from @param oldRemotePath
     *        to @param newRemotePath. Overwrites destination file if
     *        it's already present.
     *
     * @param oldRemotePath full path to file to be renamed
     * @param newRemotePath full new path to file
     */
    void rename(std::string oldRemotePath, std::string newRemotePath);

    /**
     * @brief share - create a share-link for the existing node
     * @param remotePath - path to the remote file
     * @return url to shared file as a string
     */
    std::string share(std::string remotePath);
private:

    // api helpers

    // generic

    /**
     * @brief obtainShard obtains shard for next operation. It contains url to load-balanced
     *        host to which request will be sent
     * @param type type of shard to obtain
     * @return Shard of selected type
     * @throws MailApiException in case of non-success return code
     */
    Shard obtainShard(Shard::ShardType type);

    // filesystem-related

    /**
     * @brief addUploadedFile adds uploaded file by hash to remote dir on cloud server.
     *        This operation is required after uploading a file because it makes a hard link
     *        to an uploaded file from your cloud storage.
     * @param name name of file to be added
     * @param remoteDir remote directory on cloud server
     * @param hashSize hash and size of file delimited by colon
     */
    void addUploadedFile(std::string name, std::string remoteDir, std::string hashSize);

    /**
     * @brief move - moves file from one directory to another.
     *        This operation complements @fn rename call
     *        if destination file is in another dir.
     *
     * @param whatToMove - file/folder to be moved
     * @param whereToMove - destination directory
     */
    void move(std::string whatToMove, std::string whereToMove);

    // auth
    /**
     * @brief API::authenticate - retrieves initial authentication cookies
     *
     * This actually uses provided auth details to create am authenticated session.
     * The cookies gained are stored and later reused in all operations.
     *
     * @throws MailApiException in case of auth failure
     */
    void authenticate();

    /**
     * @brief API::obtainCloudCookie - retrieves basic cloud cookie that is needed for API exchange
     * @throws MailApiException in case of failure
     */
    void obtainCloudCookie();

    /**
     * @brief API::obtainAuthToken - retrieve auth token.
     *
     * This is the first step in Mail.ru Cloud REST API exchange.
     * Then token is unique to exchange session, so it is stored and can be reused.
     *
     * @throws MailApiException in case of failure
     */
    void obtainAuthToken();

    // cURL helpers
    std::string paramString(Params const &params);
    std::string performPost();

    template<typename Container>
    void performGet(Container &target);

    std::unique_ptr<curl::curl_easy> restClient;
    curl::curl_cookie cookieStore;

    std::string proxyUrl;
    Account authAccount;
    std::string authToken;

    int64_t verbose = 0;
};

#endif // API_H
