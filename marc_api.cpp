#include "curl_header.h"
#include "curl_ios.h"

#include "marc_api.h"

#include <iterator>

#include <json/json.h>

#define NV_PAIR(name, value) curl_pair<CURLformoption, string>(CURLFORM_COPYNAME, name), \
                             curl_pair<CURLformoption, string>(CURLFORM_COPYCONTENTS, value.c_str())

using namespace std;
using namespace curl;
using namespace Json;

static const string SAFE_USER_AGENT = "Mozilla / 5.0(Windows; U; Windows NT 5.1; en - US; rv: 1.9.0.1) Gecko / 2008070208 Firefox / 3.0.1";

static const string AUTH_DOMAIN = "https://auth.mail.ru";
static const string CLOUD_DOMAIN = "https://cloud.mail.ru";

static const string AUTH_ENDPOINT = AUTH_DOMAIN + "/cgi-bin/auth";
static const string SCLD_COOKIE_ENDPOINT = AUTH_DOMAIN + "/sdc";

static const string SCLD_SHARD_ENDPOINT = CLOUD_DOMAIN + "/api/v2/dispatcher";
static const string SCLD_TOKEN_ENDPOINT = CLOUD_DOMAIN + "/api/v2/tokens/csrf";

static const string SCLD_FOLDER_ENDPOINT = CLOUD_DOMAIN + "/api/v2/folder";
static const string SCLD_FILE_ENDPOINT = CLOUD_DOMAIN + "/api/v2/file";
static const string SCLD_SPACE_ENDPOINT = CLOUD_DOMAIN + "/api/v2/user/space";

static const string SCLD_ADDFILE_ENDPOINT = SCLD_FILE_ENDPOINT + "/add";
static const string SCLD_REMOVEFILE_ENDPOINT = SCLD_FILE_ENDPOINT + "/remove";
static const string SCLD_RENAMEFILE_ENDPOINT = SCLD_FILE_ENDPOINT + "/rename";
static const string SCLD_MOVEFILE_ENDPOINT = SCLD_FILE_ENDPOINT + "/move";
static const string SCLD_ADDFOLDER_ENDPOINT = SCLD_FOLDER_ENDPOINT + "/add";

static const long MAX_FILE_SIZE = 2L * 1024L * 1024L * 1024L;

struct ReadData {
    char *data;
    size_t readIdx;
};


MarcRestClient::MarcRestClient()
    : restClient(make_unique<curl::curl_easy>()),
      cookieStore(*restClient)
{
    cookieStore.set_file(""); // init cookie engine
    restClient->reset(); // reset debug->std:cout function
}


MarcRestClient::MarcRestClient(MarcRestClient &toCopy)
    : restClient(make_unique<curl::curl_easy>(*toCopy.restClient.get())), // copy easy handle
      cookieStore(*restClient),        // cokie_store is not copyable, init in body
      authAccount(toCopy.authAccount), // copy account from other one
      authToken(toCopy.authToken) // copy auth token from other one

{
    for (const auto &c : toCopy.cookieStore.get()) {
        restClient->add<CURLOPT_COOKIELIST>(c.data());
    }
    cookieStore.set_file(""); // init cookie engine
}

string MarcRestClient::paramString(Params const &params)
{
    if(params.empty())
        return "";

    vector<string> result;
    result.reserve(params.size());
    for (auto it = params.cbegin(); it != params.end(); ++it) {
        string name = it->first, value = it->second;
        if (name.empty())
            continue;

        restClient->escape(name);
        restClient->escape(value);
        string argument = value.empty() ? name : name + "=" + value;
        result.push_back(argument);
    }

    stringstream s;
    copy(result.cbegin(), result.cend(), ostream_iterator<string>(s, "&"));
    return s.str();
}

string MarcRestClient::performPost()
{
    ostringstream stream;
    curl_ios<ostringstream> writer(stream);

    curl_header header;
    header.add("Accept: */*");
    header.add("Origin: " + CLOUD_DOMAIN);

    restClient->add<CURLOPT_HTTPHEADER>(header.get());
    restClient->add<CURLOPT_USERAGENT>(SAFE_USER_AGENT.data()); // 403 without this
    restClient->add<CURLOPT_VERBOSE>(verbose);
    restClient->add<CURLOPT_DEBUGFUNCTION>(trace_post);

    restClient->add<CURLOPT_WRITEFUNCTION>(writer.get_function());
    restClient->add<CURLOPT_WRITEDATA>(writer.get_stream());
    try {
        restClient->perform();
    } catch (curl::curl_easy_exception error) {
        curl::curlcpp_traceback errors = error.get_traceback();
        error.print_traceback();
        throw MailApiException("Couldn't perform request!");
    }
    int64_t ret = restClient->get_info<CURLINFO_RESPONSE_CODE>().get();
    if (ret != 302 && ret != 200) { // OK or redirect
        throw MailApiException("non-success return code! Error message body: " + stream.str(), ret);
    }

    restClient->reset();

    return stream.str();
}

vector<char> MarcRestClient::performGet()
{
    vector<char> result;

    curl_header header;
    header.add("Accept: */*");
    header.add("Origin: " + CLOUD_DOMAIN);

    restClient->add<CURLOPT_HTTPHEADER>(header.get());
    restClient->add<CURLOPT_USERAGENT>(SAFE_USER_AGENT.data()); // 403 without this
    restClient->add<CURLOPT_VERBOSE>(verbose);
    restClient->add<CURLOPT_FOLLOWLOCATION>(1L);
    restClient->add<CURLOPT_DEBUGFUNCTION>(trace_post);
    restClient->add<CURLOPT_WRITEDATA>(&result);
    restClient->add<CURLOPT_WRITEFUNCTION>([](void *contents, size_t size, size_t nmemb, void *userp) {
        auto result = static_cast<vector<char> *>(userp);
        char *bytes = static_cast<char *>(contents);
        const size_t realsize = size * nmemb;
        vector<char> data(&bytes[0], &bytes[realsize]);
        result->insert(result->end(), data.begin(), data.end());
        return realsize;
    });

    try {
        restClient->perform();
    } catch (curl::curl_easy_exception error) {
        curl::curlcpp_traceback errors = error.get_traceback();
        error.print_traceback();
        throw MailApiException("Couldn't perform request!");
    }
    int64_t ret = restClient->get_info<CURLINFO_RESPONSE_CODE>().get();
    if (ret != 302 && ret != 200) // OK or redirect
        throw MailApiException(string("non-success return code! Received data: ") + result.data(), ret);

    restClient->reset();

    return result;
}

void MarcRestClient::performGetAsync(BlockingQueue<char> &p)
{
    restClient->add<CURLOPT_USERAGENT>(SAFE_USER_AGENT.data()); // 403 without this
    restClient->add<CURLOPT_VERBOSE>(verbose);
    restClient->add<CURLOPT_FOLLOWLOCATION>(1L);
    restClient->add<CURLOPT_DEBUGFUNCTION>(trace_post);
    restClient->add<CURLOPT_WRITEDATA>(&p);
    restClient->add<CURLOPT_WRITEFUNCTION>([](void *contents, size_t size, size_t nmemb, void *userp) {
        auto result = static_cast<BlockingQueue<char> *>(userp);
        char * bytes = static_cast<char *>(contents);
        const size_t realsize = size * nmemb;
        vector<char> data(&bytes[0], &bytes[realsize]);
        result->push(data);
        return realsize;
    });

    try {
        restClient->perform();
    } catch (curl::curl_easy_exception error) {
        curl::curlcpp_traceback errors = error.get_traceback();
        error.print_traceback();
        throw MailApiException("Couldn't perform request!");
    }
    int64_t ret = restClient->get_info<CURLINFO_RESPONSE_CODE>().get();
    if (ret != 302 && ret != 200) // OK or redirect
        throw MailApiException("non-success return code!", ret);

    p.markEnd();
    restClient->reset();
}

bool MarcRestClient::login(const Account &acc)
{
    if (acc.login.empty())
        throw MailApiException("Login not specified!");;

    if (acc.password.empty())
        throw MailApiException("Password not specified!");

    authAccount = acc;

    authenticate();
    obtainCloudCookie();
    obtainAuthToken();

    return true;
}

/**
 * @brief API::authenticate - retrieves initial authentication cookies
 * @return true if cookies were successfully set, false otherwise
 */
void MarcRestClient::authenticate()
{
    // Login={0}&Domain={1}&Password={2}

    curl_form form;
    form.add(NV_PAIR("Login", authAccount.login));
    form.add(NV_PAIR("Password", authAccount.password));
    form.add(NV_PAIR("Domain", string("mail.ru")));

    restClient->add<CURLOPT_URL>(AUTH_ENDPOINT.data());
    restClient->add<CURLOPT_HTTPPOST>(form.get());
    performPost();

    if (cookieStore.get().empty()) // no cookies received, halt
        throw MailApiException("Failed to authenticate in mail.ru domain!");
}

/**
 * @brief API::obtainCloudCookie - retrieves basic cloud cookie that is needed for API exchange
 * @return true if cookie was successfully obtained, false otherwise
 */
void MarcRestClient::obtainCloudCookie()
{
    curl_form form;
    form.add(NV_PAIR("from", string(CLOUD_DOMAIN + "/home")));

    size_t cookiesSize = cookieStore.get().size();

    restClient->add<CURLOPT_URL>(SCLD_COOKIE_ENDPOINT.data());
    restClient->add<CURLOPT_HTTPPOST>(form.get());
    restClient->add<CURLOPT_FOLLOWLOCATION>(1L);
    performPost();

    if (cookieStore.get().size() <= cookiesSize) // didn't get any new cookies
        throw MailApiException("Failed to obtain cloud cookie, did you sign up to the cloud?");
}

/**
 * @brief API::obtainAuthToken - retrieve auth token. This is the first step in Mail.ru cloud API exchange
 * @return true if token was obtained, false otherwise
 */
void MarcRestClient::obtainAuthToken()
{
    curl_header header;
    header.add("Accept: application/json");
    
    restClient->add<CURLOPT_HTTPHEADER>(header.get());
    restClient->add<CURLOPT_URL>(SCLD_TOKEN_ENDPOINT.c_str());
    string answer = performPost();

    Value response;
    Reader reader;

    if (!reader.parse(answer, response)) // invalid JSON (shouldn't happen)
        throw MailApiException("Invalid json received from cloud endpoint!");

    if (response["body"] == Value() || response["body"]["token"] == Value())
        throw MailApiException("Received json doesn't contain token string!");

    authToken = response["body"]["token"].asString();
}

Shard MarcRestClient::obtainShard(Shard::ShardType type)
{
    using Json::Value;

    curl_header header;
    header.add("Accept: application/json");

    string url = SCLD_SHARD_ENDPOINT + "?" + paramString({{"token", authToken}});
    restClient->add<CURLOPT_URL>(url.data());
    restClient->add<CURLOPT_HTTPHEADER>(header.get());
    string answer = performPost();

    Value response;
    Json::Reader reader;

    if (!reader.parse(answer, response)) // invalid JSON (shouldn't happen)
        throw MailApiException("Error parsing shard response JSON");

    if (response["body"] != Value()) {
        return Shard(response["body"][Shard::asString(type)]);
    }

    throw MailApiException("Non-Shard json received: " + answer);
}

void MarcRestClient::addUploadedFile(string name, string remoteDir, string hashSize)
{
    auto index = hashSize.find(';');
    auto endIndex = hashSize.find('\r');
    if (index == string::npos)
        throw MailApiException("Non-hashsize answer received: " + hashSize);

    string fileHash = hashSize.substr(0, index);
    string fileSize = hashSize.substr(index + 1, endIndex - (index + 1));

    curl_header header;
    header.add("Accept: */*");
    header.add("Origin: " + CLOUD_DOMAIN);

    string postFields = paramString({
        {"api", "2"},
        {"conflict", "rewrite"}, // rename is one more discovered option
        {"home", remoteDir + name},
        {"hash", fileHash},
        {"size", fileSize},
        {"token", authToken}
    });

    restClient->add<CURLOPT_URL>(SCLD_ADDFILE_ENDPOINT.data());
    restClient->add<CURLOPT_FOLLOWLOCATION>(1L);
    restClient->add<CURLOPT_HTTPHEADER>(header.get());
    restClient->add<CURLOPT_POSTFIELDS>(postFields.data());
    performPost();
}

void MarcRestClient::move(string whatToMove, string whereToMove)
{
    string postFields = paramString({
        {"api", "2"},
        {"conflict", "rewrite"},  // rewrite is one more discovered option
        {"folder", whereToMove},
        {"home", whatToMove},
        {"token", authToken}
    });

    restClient->add<CURLOPT_URL>(SCLD_MOVEFILE_ENDPOINT.data());
    restClient->add<CURLOPT_FOLLOWLOCATION>(1L);
    restClient->add<CURLOPT_POSTFIELDS>(postFields.data());
    performPost();
}

void MarcRestClient::remove(string remotePath)
{
    string postFields = paramString({
        {"api", "2"},
        {"home", remotePath},
        {"token", authToken}
    });

    restClient->add<CURLOPT_URL>(SCLD_REMOVEFILE_ENDPOINT.data());
    restClient->add<CURLOPT_FOLLOWLOCATION>(1L);
    restClient->add<CURLOPT_POSTFIELDS>(postFields.data());
    performPost();
}

SpaceInfo MarcRestClient::df()
{
    curl_header header;
    header.add("Accept: application/json");

    string getFields = paramString({
        {"api", "2"},
        {"token", authToken},
    });

    restClient->add<CURLOPT_URL>((SCLD_SPACE_ENDPOINT + "?" + getFields).data());
    restClient->add<CURLOPT_FOLLOWLOCATION>(1L);
    restClient->add<CURLOPT_HTTPHEADER>(header.get());
    string answerJson = performPost();

    SpaceInfo result;
    Value response;
    Reader reader;

    if (!reader.parse(answerJson, response)) // invalid JSON (shouldn't happen)
        throw MailApiException("Cannot parse JSON ls response!");

    // if `total` is there, `used` will definitely be...
    if (response["body"] == Value() || response["body"]["total"] == Value())
        throw MailApiException("Non-well formed JSON ls response!");

    Value &total = response["body"]["total"];
    Value &used = response["body"]["used"];

    result.totalMiB = total.asUInt64();
    result.usedMiB = used.asUInt();
    return result;
}

void MarcRestClient::rename(string oldRemotePath, string newRemotePath)
{
    string oldFilename = oldRemotePath.substr(oldRemotePath.find_last_of("/\\") + 1);
    string oldParentDir = oldRemotePath.substr(0, oldRemotePath.find_last_of("/\\") + 1);

    string newFilename = newRemotePath.substr(newRemotePath.find_last_of("/\\") + 1);
    string newParentDir = newRemotePath.substr(0, newRemotePath.find_last_of("/\\") + 1);

    string postFields = paramString({
        {"api", "2"},
        {"conflict", "rewrite"}, // rename is one more discovered option
        {"home", oldRemotePath},
        {"name", newFilename},
        {"token", authToken}
    });

    restClient->add<CURLOPT_URL>(SCLD_RENAMEFILE_ENDPOINT.data());
    restClient->add<CURLOPT_FOLLOWLOCATION>(1L);
    restClient->add<CURLOPT_POSTFIELDS>(postFields.data());
    performPost();

    // FIXME: think about version that doesn't rewrite file with name == newFilename
    // placed in the old dir.

    if (oldParentDir != newParentDir) {
        move(oldParentDir + newFilename, newParentDir);
    }
}

void MarcRestClient::upload(string remotePath, vector<char>& data)
{
    Shard s = obtainShard(Shard::ShardType::UPLOAD);

    string filename = remotePath.substr(remotePath.find_last_of("/\\") + 1);
    string parentDir = remotePath.substr(0, remotePath.find_last_of("/\\") + 1);

    // zero size upload requested, skip upload part completely
    if (data.empty()) {
        // add zero file, special hash
        addUploadedFile(filename, parentDir, "0000000000000000000000000000000000000000;0");
        return;
    }

    string uploadUrl = s.getUrl() + "?" + paramString({{"cloud_domain", "2"}, {"x-email", authAccount.login}});

    // fileupload part
    curl_form nameForm;
    ReadData ptr {&data[0], 0};
    nameForm.add(curl_pair<CURLformoption, string>(CURLFORM_COPYNAME, "file"),
                 curl_pair<CURLformoption, string>(CURLFORM_FILENAME, filename),
                 curl_pair<CURLformoption, char *>(CURLFORM_STREAM, reinterpret_cast<char *>(&ptr)),
                 curl_pair<CURLformoption, long>(CURLFORM_CONTENTSLENGTH, static_cast<long>(data.size())));

    restClient->add<CURLOPT_URL>(uploadUrl.data());
    restClient->add<CURLOPT_FOLLOWLOCATION>(1L);
    restClient->add<CURLOPT_HTTPPOST>(nameForm.get());
    restClient->add<CURLOPT_READFUNCTION>([](void *contents, size_t size, size_t nmemb, void *userp) {
        auto source = static_cast<ReadData *>(userp);
        auto target = static_cast<char *>(contents);
        const size_t requested = size * nmemb;
        copy_n(&source->data[source->readIdx], requested, target);
        source->readIdx += requested;
        return requested;
    });
    string answer = performPost();

    addUploadedFile(filename, parentDir, answer);
}

void MarcRestClient::uploadAsync(string remotePath, BlockingQueue<char> &p)
{
    Shard s = obtainShard(Shard::ShardType::UPLOAD);

    string filename = remotePath.substr(remotePath.find_last_of("/\\") + 1);
    string parentDir = remotePath.substr(0, remotePath.find_last_of("/\\") + 1);
    string uploadUrl = s.getUrl() + "?" + paramString({{"cloud_domain", "2"}, {"x-email", authAccount.login}});

    //header.add("Transfer-Encoding: chunked"); // we don't know exact size of the upload...
    //header.add("Content-Length: 0");

    curl_form nameForm; // streamupload part
    /*
    nameForm.add(curl_pair<CURLformoption, string>(CURLFORM_COPYNAME, "file"),
                 curl_pair<CURLformoption, string>(CURLFORM_FILENAME, filename),
                 curl_pair<CURLformoption, void *>(CURLFORM_STREAM, &p),
                 curl_pair<CURLformoption, long>(CURLFORM_CONTENTSLENGTH, 1)); // replace with CURLFORM_CONTENTLEN in future
    */
    restClient->add<CURLOPT_URL>(uploadUrl.data());
    restClient->add<CURLOPT_FOLLOWLOCATION>(1L);
    restClient->add<CURLOPT_HTTPPOST>(nameForm.get());
    restClient->add<CURLOPT_READFUNCTION>([](void *contents, size_t size, size_t nmemb, void *userp) {
        auto result = static_cast<BlockingQueue<char> *>(userp);
        char *target = static_cast<char *>(contents);
        const size_t requested = size * nmemb;
        size_t transferred = 0;
        while (!result->exhausted()) { // try to pull from the other end of queue
            transferred += result->pop(target + transferred, requested - transferred);
            if (transferred == requested)
                break;
        }
        return transferred;
    });
    string answer = performPost();

    addUploadedFile(filename, parentDir, answer);
}

void MarcRestClient::mkdir(string remotePath)
{
    string postFields = paramString({
        {"api", "2"},
        {"conflict", "rewrite"},  // rename is one more discovered option
        {"home", remotePath},
        {"token", authToken}
    });

    restClient->add<CURLOPT_URL>(SCLD_ADDFOLDER_ENDPOINT.data());
    restClient->add<CURLOPT_FOLLOWLOCATION>(1L);
    restClient->add<CURLOPT_POSTFIELDS>(postFields.data());
    performPost();
}

vector<CloudFile> MarcRestClient::ls(string remotePath)
{
    curl_header header;
    header.add("Accept: application/json");

    string getFields = paramString({
        {"api", "2"},
        {"token", authToken},
        {"home", remotePath}
    });

    restClient->add<CURLOPT_URL>((SCLD_FOLDER_ENDPOINT + "?" + getFields).data());
    restClient->add<CURLOPT_FOLLOWLOCATION>(1L);
    restClient->add<CURLOPT_HTTPHEADER>(header.get());
    string answerJson = performPost();

    vector<CloudFile> results;
    Value response;
    Reader reader;

    if (!reader.parse(answerJson, response)) // invalid JSON (shouldn't happen)
        throw MailApiException("Cannot parse JSON ls response!");
    
    if (response["body"] == Value() || response["body"]["list"] == Value())
        throw MailApiException("Non-well formed JSON ls response!");

    Value &list = response["body"]["list"];
    for (const Value &entry : list) {
        results.push_back(CloudFile(entry));
    }

    return results;
}

std::vector<char> MarcRestClient::download(string remotePath)
{
    Shard s = obtainShard(Shard::ShardType::GET);
    restClient->add<CURLOPT_URL>((s.getUrl() + remotePath).data());
    return performGet();
}

void MarcRestClient::downloadAsync(string remotePath, BlockingQueue<char> &p)
{
    Shard s = obtainShard(Shard::ShardType::GET);
    restClient->add<CURLOPT_URL>((s.getUrl() + remotePath).data());
    return performGetAsync(p);
}
