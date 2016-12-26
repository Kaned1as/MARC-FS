#include "curl_header.h"
#include "curl_ios.h"

#include "api.h"
#include "utils.h"

#include <numeric>
#include <iterator>
#include <string>

#include <json/json.h>

#define NV_PAIR(name, value) curl_pair<CURLformoption, string>(CURLFORM_COPYNAME, name), \
                             curl_pair<CURLformoption, string>(CURLFORM_COPYCONTENTS, value.c_str())

#define NV_FILEPAIR(name, value) curl_pair<CURLformoption, string>(CURLFORM_COPYNAME, name), \
    curl_pair<CURLformoption, string>(CURLFORM_FILE, value.c_str())

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
static const string SCLD_ADDFILE_ENDPOINT = CLOUD_DOMAIN + "/api/v2/file/add";
static const string SCLD_ADDFOLDER_ENDPOINT = CLOUD_DOMAIN + SCLD_FOLDER_ENDPOINT + "/add";

static const long MAX_FILE_SIZE = 2L * 1000L * 1000L * 1000L;

string API::paramString(Params const &params)
{
    if(params.empty())
        return "";

    vector<string> result;
    result.reserve(params.size());
    for (auto it = params.cbegin(); it != params.end(); ++it) {
        string name = it->first, value = it->second;
        if (name.empty())
            continue;

        mClient->escape(name);
        mClient->escape(value);
        string argument = value.empty() ? name : name + "=" + value;
        result.push_back(argument);
    }

    stringstream s;
    copy(result.cbegin(), result.cend(), ostream_iterator<string>(s, "&"));
    return s.str();
}

string API::performPost()
{
    ostringstream stream;
    curl_ios<ostringstream> writer(stream);

    mClient->add<CURLOPT_USERAGENT>(SAFE_USER_AGENT.data()); // 403 without this
    mClient->add<CURLOPT_VERBOSE>(1L);
    mClient->add<CURLOPT_DEBUGFUNCTION>(trace_post);

    mClient->add<CURLOPT_WRITEFUNCTION>(writer.get_function());
    mClient->add<CURLOPT_WRITEDATA>(writer.get_stream());
    try {
        mClient->perform();
    } catch (curl::curl_easy_exception error) {
        curl::curlcpp_traceback errors = error.get_traceback();
        error.print_traceback();
        throw MailApiException("Couldn't perform request!");
    }
    int64_t ret = mClient->get_info<CURLINFO_RESPONSE_CODE>().get();
    if (ret != 302 && ret != 200) // OK or redirect
        throw MailApiException("non-success return code!");

    mClient->reset();

    return stream.str();
}

void API::postAsync(Pipe &p)
{
    mClient->add<CURLOPT_USERAGENT>(SAFE_USER_AGENT.data()); // 403 without this
    mClient->add<CURLOPT_VERBOSE>(1L);
    mClient->add<CURLOPT_DEBUGFUNCTION>(trace_post);

    mClient->add<CURLOPT_WRITEDATA>(&p);
    mClient->add<CURLOPT_WRITEFUNCTION>([](void *contents, size_t size, size_t nmemb, void *userp) {
        auto result = static_cast<Pipe*>(userp);
        unsigned char * bytes = static_cast<unsigned char *>(contents);
        const size_t realsize = size * nmemb;
        vector<int8_t> data(&bytes[0], &bytes[realsize]);
        result->push(data);
        return realsize;
    });
    try {
        mClient->perform();
    } catch (curl::curl_easy_exception error) {
        curl::curlcpp_traceback errors = error.get_traceback();
        error.print_traceback();
        throw MailApiException("Couldn't perform request!");
    }
    int64_t ret = mClient->get_info<CURLINFO_RESPONSE_CODE>().get();
    if (ret != 302 && ret != 200) // OK or redirect
        throw MailApiException("non-success return code!");

    mClient->reset();
    p.markEnd();
}

void API::getAsync(API::Pipe &p)
{

}

vector<int8_t> API::performGet()
{
    vector<int8_t> result;
    mClient->add<CURLOPT_USERAGENT>(SAFE_USER_AGENT.data()); // 403 without this
    mClient->add<CURLOPT_VERBOSE>(1L);
    mClient->add<CURLOPT_FOLLOWLOCATION>(1L);
    mClient->add<CURLOPT_DEBUGFUNCTION>(trace_post);
    mClient->add<CURLOPT_WRITEDATA>(&result);
    mClient->add<CURLOPT_WRITEFUNCTION>([](void *contents, size_t size, size_t nmemb, void *userp) {
        auto result = static_cast<vector<int8_t>*>(userp);
        unsigned char * bytes = static_cast<unsigned char *>(contents);
        const size_t realsize = size * nmemb;
        vector<int8_t> data(&bytes[0], &bytes[realsize]);
        result->insert(result->end(), data.begin(), data.end());
        return realsize;
    });

    try {
        mClient->perform();
    } catch (curl::curl_easy_exception error) {
        curl::curlcpp_traceback errors = error.get_traceback();
        error.print_traceback();
        throw MailApiException("Couldn't perform request!");
    }
    int64_t ret = mClient->get_info<CURLINFO_RESPONSE_CODE>().get();
    if (ret != 302 && ret != 200) // OK or redirect
        throw MailApiException("non-success return code!");

    mClient->reset();

    return result;
}

API::API()
    : mClient(make_unique<curl::curl_easy>()),
      mCookies(*mClient)
{
    mCookies.set_file(""); // init cookie engine
}

bool API::login(const Account &acc)
{
    if (acc.login.empty())
        throw MailApiException("Login not specified!");;

    if (acc.password.empty())
        throw MailApiException("Password not specified!");

    mAccount = acc;

    authenticate();
    obtainCloudCookie();
    obtainAuthToken();

    return true;
}

/**
 * @brief API::authenticate - retrieves initial authentication cookies
 * @return true if cookies were successfully set, false otherwise
 */
void API::authenticate()
{
    // Login={0}&Domain={1}&Password={2}

    curl_form form;
    form.add(NV_PAIR("Login", mAccount.login));
    form.add(NV_PAIR("Password", mAccount.password));
    form.add(NV_PAIR("Domain", string("mail.ru")));

    mClient->add<CURLOPT_URL>(AUTH_ENDPOINT.data());
    mClient->add<CURLOPT_HTTPPOST>(form.get());
    performPost();

    if (mCookies.get().empty()) // no cookies received, halt
        throw MailApiException("Failed to authenticate in mail.ru domain!");
}

/**
 * @brief API::obtainCloudCookie - retrieves basic cloud cookie that is needed for API exchange
 * @return true if cookie was successfully obtained, false otherwise
 */
void API::obtainCloudCookie()
{
    curl_form form;
    form.add(NV_PAIR("from", string(CLOUD_DOMAIN + "/home")));

    size_t cookiesSize = mCookies.get().size();

    mClient->add<CURLOPT_URL>(SCLD_COOKIE_ENDPOINT.data());
    mClient->add<CURLOPT_HTTPPOST>(form.get());
    mClient->add<CURLOPT_FOLLOWLOCATION>(1L);
    performPost();

    if (mCookies.get().size() <= cookiesSize) // didn't get any new cookies
        throw MailApiException("Failed to obtain cloud cookie, did you sign up to the cloud?");
}

/**
 * @brief API::obtainAuthToken - retrieve auth token. This is the first step in Mail.ru cloud API exchange
 * @return true if token was obtained, false otherwise
 */
void API::obtainAuthToken()
{
    curl_header header;
    header.add("Accept: application/json");
    
    mClient->add<CURLOPT_HTTPHEADER>(header.get());
    mClient->add<CURLOPT_URL>(SCLD_TOKEN_ENDPOINT.c_str());
    string answer = performPost();

    Value response;
    Reader reader;

    if (!reader.parse(answer, response)) // invalid JSON (shouldn't happen)
        throw MailApiException("Invalid json received from cloud endpoint!");

    if (response["body"] == Value() || response["body"]["token"] == Value())
        throw MailApiException("Received json doesn't contain token string!");

    mToken = response["body"]["token"].asString();
}

Shard API::obtainShard(Shard::ShardType type)
{
    using Json::Value;

    curl_header header;
    header.add("Accept: application/json");

    string url = SCLD_SHARD_ENDPOINT + "?" + paramString({{"token", mToken}});
    mClient->add<CURLOPT_URL>(url.data());
    mClient->add<CURLOPT_HTTPHEADER>(header.get());
    string answer = performPost();

    Value response;
    Json::Reader reader;

    if (!reader.parse(answer, response)) // invalid JSON (shouldn't happen)
        throw MailApiException("Error parsing shard response JSON");

    if (response["body"] != Value()) {
        return Shard(response["body"][Shard::as_string(type)]);
    }

    throw MailApiException("Non-Shard json received: " + answer);
}

void API::addUploadedFile(string name, string remoteDir, string hashSize)
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
    header.add("Referer: " + CLOUD_DOMAIN + "/home" + remoteDir);

    string postFields = paramString({
        {"api", "2"},
        {"conflict", "rename"},
        {"home", remoteDir + name},
        {"hash", fileHash},
        {"size", fileSize},
        {"token", mToken}
    });

    mClient->add<CURLOPT_URL>(SCLD_ADDFILE_ENDPOINT.data());
    mClient->add<CURLOPT_FOLLOWLOCATION>(1L);
    mClient->add<CURLOPT_HTTPHEADER>(header.get());
    mClient->add<CURLOPT_POSTFIELDS>(postFields.data());
    performPost();
}


void API::upload(std::vector<int8_t> data, string remotePath)
{
    Shard s = obtainShard(Shard::ShardType::UPLOAD);

    string filename = remotePath.substr(remotePath.find_last_of("/\\") + 1);
    string uploadUrl = s.getUrl() + "?" + paramString({{"cloud_domain", "2"}, {"x-email", mAccount.login}});

    curl_header header;
    header.add("Accept: */*");
    header.add("Origin: " + CLOUD_DOMAIN);
    header.add("Referer: " + CLOUD_DOMAIN + "/home" + remotePath);

    curl_form nameForm;
    nameForm.add(NV_FILEPAIR("file", data)); // fileupload part
    nameForm.add(NV_PAIR("_file", filename)); // naming part

    curl_form file_form;

    mClient->add<CURLOPT_URL>(uploadUrl.data());
    mClient->add<CURLOPT_FOLLOWLOCATION>(1L);
    mClient->add<CURLOPT_HTTPPOST>(nameForm.get());
    mClient->add<CURLOPT_HTTPHEADER>(header.get());
    string answer = performPost();

    addUploadedFile(filename, remotePath, answer);

    return;
}

void API::mkdir(string remotePath)
{
    curl_header header;
    header.add("Accept: */*");
    header.add("Origin: " + CLOUD_DOMAIN);
    header.add("Referer: " + CLOUD_DOMAIN + "/home" + remotePath);

    string postFields = paramString({
        {"api", "2"},
        {"conflict", "rename"},
        {"home", remotePath},
        {"token", mToken}
    });

    mClient->add<CURLOPT_URL>(SCLD_ADDFOLDER_ENDPOINT.data());
    mClient->add<CURLOPT_FOLLOWLOCATION>(1L);
    mClient->add<CURLOPT_HTTPHEADER>(header.get());
    mClient->add<CURLOPT_POSTFIELDS>(postFields.data());
    performPost();
}

vector<CloudFile> API::ls(string remotePath)
{
    curl_header header;
    header.add("Accept: application/json");

    string getFields = paramString({
        {"token", mToken},
        {"home", remotePath}
    });

    mClient->add<CURLOPT_URL>((SCLD_FOLDER_ENDPOINT + "?" + getFields).data());
    mClient->add<CURLOPT_FOLLOWLOCATION>(1L);
    mClient->add<CURLOPT_HTTPHEADER>(header.get());
    string answerJson = performPost();
    cout << answerJson;

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

vector<int8_t> API::download(string remotePath)
{
    Shard s = obtainShard(Shard::ShardType::GET);
    mClient->add<CURLOPT_URL>((s.getUrl() + remotePath).data());
    return  performGet();
}
