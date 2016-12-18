#ifndef API_H
#define API_H

#include "account.h"
#include "api_shard.h"
#include "api_cloudfile.h"

#include "curl_easy.h"
#include "curl_cookie.h"

#include <memory>

using namespace std;
using namespace curl;
using namespace Json;

class API
{
    using Params = map<string, string>;

public:
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
    void upload(string path, string remotePath);

    /**
     * @brief mkdir creates a directory noted by remotePath
     * @param remote_path new directory absolute path (e.g. /home/newfolder)
     */
    void mkdir(string remotePath);

    /**
     * @brief ls list entries in a directory
     * @param remote_path absolute path to directory to list
     * @return vector of file items
     */
    vector<CloudFile> ls(string remotePath);

    /**
     * @brief download download file pointed by remotePath to local path
     * @param remotePath remote path on cloud server
     * @param path path on local machine
     */
    vector<int8_t> download(string remotePath);
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
    void addUploadedFile(string name, string remoteDir, string hashSize);

    // auth
    void authenticate();
    void obtainCloudCookie();
    void obtainAuthToken();

    // cURL helpers
    string paramString(Params const &params);
    string performPost(bool reset = true);
    vector<int8_t> performGet();

    Account mAccount;
    string mToken;

    unique_ptr<curl::curl_easy> mClient;
    curl::curl_cookie mCookies;
};

#endif // API_H
