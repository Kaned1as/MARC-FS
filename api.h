#ifndef API_H
#define API_H

#include "account.h"
#include "api_shard.h"
#include "api_cloudfile.h"

#include "curl_easy.h"
#include "curl_cookie.h"

#include "blocking_queue.h"

#include <memory>

class API
{
public:
    using Params = std::map<std::string, std::string>;

    API();

    /**
     * @brief API::login Sends auth info and initializes this API object on successful login.
     * @param acc account to auth with
     * @return true if authenticated successfully, false otherwise.
     */
    bool login(const Account& acc);
    /**
     * @brief upload uploads the file represented in path to the remote dir represented by remotePath
     * @param path path to local file (e.g. 123.cpp or /home/user/123.cpp)
     * @param remote_path remote path to folder where uploaded file should be (e.g. /home/newfolder)
     */
    void upload(std::vector<char> data, std::string remotePath);

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
     * @param path path on local machine
     */
    std::vector<char> download(std::string remotePath);
    void downloadAsync(std::string remotePath, BlockingQueue<char> &p);

    /**
     * @brief remove removes file pointed by remotePath from cloud storage
     * @param remotePath remote path on cloud server
     */
    void remove(std::string remotePath);
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

    // auth
    void authenticate();
    void obtainCloudCookie();
    void obtainAuthToken();

    // cURL helpers
    std::string paramString(Params const &params);
    std::string performPost();
    std::vector<char> performGet();
    void performGetAsync(BlockingQueue<char> &p);

    void postAsync(BlockingQueue<char> &p);

    Account mAccount;
    std::string mToken;

    std::unique_ptr<curl::curl_easy> mClient;
    curl::curl_cookie mCookies;

    int64_t verbose = 0;
};

#endif // API_H
