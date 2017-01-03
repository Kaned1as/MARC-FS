#ifndef API_H
#define API_H

#include "account.h"
#include "marc_api_shard.h"
#include "marc_api_cloudfile.h"

#include "curl_easy.h"
#include "curl_cookie.h"

#include "blocking_queue.h"
#include "utils.h"

#include <memory>

class MarcRestClient
{
public:
    using Params = std::map<std::string, std::string>;

    MarcRestClient();

    /**
     * @brief API::login Sends auth info and initializes this API object on successful login.
     * @param acc account to auth with
     * @return true if authenticated successfully, false otherwise.
     */
    bool login(const Account& acc);

    /**
     * @brief upload uploads bytes in @param data to remote endpoint
     * @param remotePath remote path to folder where uploaded file should be (e.g. /newfolder)
     */
    void upload(std::string remotePath, std::vector<char> &data);

    /**
     * @brief uploadAsync - async version of @fn upload call
     *
     * @note DOES NOT WORK - REST API's nginx server doesn't support chunked POST
     * requests.
     *
     * @param remotePath - remote path
     * @param p - pipe to read data from
     */
    void uploadAsync(std::string remotePath, BlockingQueue<char> &p);

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
     * @return data downloaded from server
     */
    std::vector<char> download(std::string remotePath);

    /**
     * @brief downloadAsync - async version of @fn download call
     * @param remotePath - path from where download file to (e.g. /file.txt)
     * @param p - pipe to store downloaded bytes to
     */
    void downloadAsync(std::string remotePath, BlockingQueue<char> &p);

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
    void authenticate();
    void obtainCloudCookie();
    void obtainAuthToken();

    // cURL helpers
    std::string paramString(Params const &params);
    std::string performPost();
    std::vector<char> performGet();
    void performGetAsync(BlockingQueue<char> &p);
    void performPostAsync(BlockingQueue<char> &p);

    Account authAccount;
    std::string authToken;

    std::unique_ptr<curl::curl_easy> restClient;
    curl::curl_cookie cookieStore;

    int64_t verbose = 0;
};

#endif // API_H
