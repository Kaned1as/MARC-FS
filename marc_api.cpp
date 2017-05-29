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

#include <iterator>

#include <json/json.h>

#include "curl_header.h"

#include "marc_api.h"
#include "abstract_storage.h"

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
static const string SCLD_PUBLISHFILE_ENDPOINT = SCLD_FILE_ENDPOINT + "/publish";
static const string SCLD_REMOVEFILE_ENDPOINT = SCLD_FILE_ENDPOINT + "/remove";
static const string SCLD_RENAMEFILE_ENDPOINT = SCLD_FILE_ENDPOINT + "/rename";
static const string SCLD_MOVEFILE_ENDPOINT = SCLD_FILE_ENDPOINT + "/move";
static const string SCLD_ADDFOLDER_ENDPOINT = SCLD_FOLDER_ENDPOINT + "/add";

const string SCLD_PUBLICLINK_ENDPOINT = CLOUD_DOMAIN + "/public";

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
      proxyUrl(toCopy.proxyUrl),        // copy proxy URL
      authAccount(toCopy.authAccount), // copy account from other one
      authToken(toCopy.authToken) // copy auth token from other one

{
    for (const auto &c : toCopy.cookieStore.get()) {
        restClient->add<CURLOPT_COOKIELIST>(c.data());
    }
    cookieStore.set_file(""); // init cookie engine
}

void MarcRestClient::setProxy(string proxyUrl)
{
    this->proxyUrl = proxyUrl;
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

    ScopeGuard resetter = [&] { restClient->reset(); };
    if (!proxyUrl.empty())
        restClient->add<CURLOPT_PROXY>(proxyUrl.data());
    restClient->add<CURLOPT_HTTPHEADER>(header.get());
    restClient->add<CURLOPT_FOLLOWLOCATION>(1L);
    restClient->add<CURLOPT_USERAGENT>(SAFE_USER_AGENT.data()); // 403 without this
    restClient->add<CURLOPT_VERBOSE>(verbose);
    restClient->add<CURLOPT_DEBUGFUNCTION>(trace_post);

    restClient->add<CURLOPT_WRITEFUNCTION>(writer.get_function());
    restClient->add<CURLOPT_WRITEDATA>(writer.get_stream());
    try {
        restClient->perform();
    } catch (curl::curl_easy_exception error) {
        error.print_traceback();
        throw MailApiException("Couldn't perform request!");
    }
    int64_t ret = restClient->get_info<CURLINFO_RESPONSE_CODE>().get();
    if (ret != 302 && ret != 200) { // OK or redirect
        throw MailApiException("non-success return code! Error message body: " + stream.str(), ret);
    }

    return stream.str();
}

template<typename Container>
void MarcRestClient::performGet(Container &target)
{
    curl_header header;
    header.add("Accept: */*");
    header.add("Origin: " + CLOUD_DOMAIN);

    ScopeGuard resetter = [&] { restClient->reset(); };
    if (!proxyUrl.empty())
        restClient->add<CURLOPT_PROXY>(proxyUrl.data());
    restClient->add<CURLOPT_HTTPHEADER>(header.get());
    restClient->add<CURLOPT_USERAGENT>(SAFE_USER_AGENT.data()); // 403 without this
    restClient->add<CURLOPT_VERBOSE>(verbose);
    restClient->add<CURLOPT_FOLLOWLOCATION>(1L);
    restClient->add<CURLOPT_DEBUGFUNCTION>(trace_post);
    restClient->add<CURLOPT_WRITEDATA>(&target);
    restClient->add<CURLOPT_WRITEFUNCTION>([](void *contents, size_t size, size_t nmemb, void *userp) {
        auto result = static_cast<Container *>(userp);
        char *bytes = static_cast<char *>(contents);
        const size_t realsize = size * nmemb;
        result->append(bytes, realsize);
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
    if (ret != 302 && ret != 200) { // OK or redirect
        if (target.empty())
            throw MailApiException("Non-success return code!", ret);

        string body = target.readFully();
        throw MailApiException(string("Non-success return code! Body:") + body, ret);
    }
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

void MarcRestClient::create(string remotePath)
{
    string filename = remotePath.substr(remotePath.find_last_of("/\\") + 1);
    string parentDir = remotePath.substr(0, remotePath.find_last_of("/\\") + 1);

    // add zero file, special hash
    addUploadedFile(filename, parentDir, "0000000000000000000000000000000000000000;0");
}

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
        throw MailApiException("Failed to authenticate " + authAccount.login + " in mail.ru domain!");
}

void MarcRestClient::obtainCloudCookie()
{
    curl_form form;
    form.add(NV_PAIR("from", string(CLOUD_DOMAIN + "/home")));

    size_t cookiesSize = cookieStore.get().size();

    restClient->add<CURLOPT_URL>(SCLD_COOKIE_ENDPOINT.data());
    restClient->add<CURLOPT_HTTPPOST>(form.get());
    performPost();

    if (cookieStore.get().size() <= cookiesSize) // didn't get any new cookies
        throw MailApiException("Failed to obtain cloud cookie, did you sign up to the cloud?");
}

void MarcRestClient::obtainAuthToken()
{
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

    string url = SCLD_SHARD_ENDPOINT + "?" + paramString({{"token", authToken}});
    restClient->add<CURLOPT_URL>(url.data());
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

    string postFields = paramString({
        {"api", "2"},
        {"conflict", "rewrite"}, // rename is one more discovered option
        {"home", remoteDir + name},
        {"hash", fileHash},
        {"size", fileSize},
        {"token", authToken}
    });

    restClient->add<CURLOPT_URL>(SCLD_ADDFILE_ENDPOINT.data());
    restClient->add<CURLOPT_POSTFIELDS>(postFields.data());
    performPost();
}

void MarcRestClient::move(string whatToMove, string whereToMove)
{
    string postFields = paramString({
        {"api", "2"},
        {"conflict", "rewrite"},  // rename is one more discovered option
        {"folder", whereToMove},
        {"home", whatToMove},
        {"token", authToken}
    });

    restClient->add<CURLOPT_URL>(SCLD_MOVEFILE_ENDPOINT.data());
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
    restClient->add<CURLOPT_POSTFIELDS>(postFields.data());
    performPost();
}

SpaceInfo MarcRestClient::df()
{

    string getFields = paramString({
        {"api", "2"},
        {"token", authToken},
    });

    restClient->add<CURLOPT_URL>((SCLD_SPACE_ENDPOINT + "?" + getFields).data());
    string answerJson = performPost();

    SpaceInfo result;
    Value response;
    Reader reader;

    if (!reader.parse(answerJson, response)) // invalid JSON (shouldn't happen)
        throw MailApiException("Cannot parse JSON df response!");

    // if `total` is there, `used` will definitely be...
    if (response["body"] == Value() || response["body"]["total"] == Value())
        throw MailApiException("Non-well formed JSON df response!");

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
    restClient->add<CURLOPT_POSTFIELDS>(postFields.data());
    performPost();

    // FIXME: think about version that doesn't rewrite file with name == newFilename
    // placed in the old dir.

    if (oldParentDir != newParentDir) {
        move(oldParentDir + newFilename, newParentDir);
    }
}

string MarcRestClient::share(string remotePath)
{
    string postFields = paramString({
        {"api", "2"},
        {"home", remotePath},
        {"token", authToken},
    });

    restClient->add<CURLOPT_URL>(SCLD_PUBLISHFILE_ENDPOINT.data());
    restClient->add<CURLOPT_POSTFIELDS>(postFields.data());
    string answerJson = performPost();

    Value response;
    Reader reader;

    if (!reader.parse(answerJson, response)) // invalid JSON (shouldn't happen)
        throw MailApiException("Cannot parse JSON share response!");

    if (response["body"] == Value())
        throw MailApiException("Non-well formed JSON share response!");

    return response["body"].asString();
}

template <typename Container>
struct ReadData {
    /*const*/ Container * const content; // content to read from
    size_t offset; // current offset of read
    size_t count; // maximum offset - can be lower than content.size()
};

template<typename Container>
void MarcRestClient::upload(string remotePath, Container &body, size_t start, size_t count)
{
    static_assert(is_same<Container, vector<char>>::value ||
                  is_same<Container, AbstractStorage>::value, "Container should be of compatible container type");

    if (body.empty()) {
        // zero size upload requested, skip upload part completely
        create(remotePath);
        return;
    }

    string filename = remotePath.substr(remotePath.find_last_of("/\\") + 1);
    string parentDir = remotePath.substr(0, remotePath.find_last_of("/\\") + 1);

    Shard s = obtainShard(Shard::ShardType::UPLOAD);
    string uploadUrl = s.getUrl() + "?" + paramString({{"cloud_domain", "2"}, {"x-email", authAccount.login}});

    // fileupload part
    curl_form nameForm;
    size_t realSize = min(body.size() - start, count); // size to transfer
    ReadData<Container> ptr {&body, start, min(body.size(), start + count)};
    nameForm.add(curl_pair<CURLformoption, string>(CURLFORM_COPYNAME, "file"),
                 curl_pair<CURLformoption, string>(CURLFORM_FILENAME, filename),
                 curl_pair<CURLformoption, char *>(CURLFORM_STREAM, reinterpret_cast<char *>(&ptr)),
                 curl_pair<CURLformoption, long>(CURLFORM_CONTENTSLENGTH, static_cast<long>(realSize)));

    // done via READFUNCTION because BUFFERPTR copies data inside cURL lib
    restClient->add<CURLOPT_URL>(uploadUrl.data());
    restClient->add<CURLOPT_HTTPPOST>(nameForm.get());
    restClient->add<CURLOPT_READFUNCTION>([](void *contents, size_t size, size_t nmemb, void *userp) {
        auto source = static_cast<ReadData<Container> *>(userp);
        auto target = static_cast<char *>(contents);
        const size_t requested = size * nmemb;
        const size_t available = source->count - source->offset;
        const size_t transferred = min(requested, available);
        source->content->read(target, transferred, source->offset);
        source->offset += transferred;
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
    restClient->add<CURLOPT_POSTFIELDS>(postFields.data());
    performPost();
}

vector<CloudFile> MarcRestClient::ls(string remotePath)
{
    string getFields = paramString({
        {"api", "2"},
        {"limit", "100500" }, // 100500 files in folder - who'd dare for more?
        {"token", authToken},
        {"home", remotePath}
    });

    restClient->add<CURLOPT_URL>((SCLD_FOLDER_ENDPOINT + "?" + getFields).data());
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

template<typename Container>
void MarcRestClient::download(string remotePath, Container& target)
{
    Shard s = obtainShard(Shard::ShardType::GET);
    restClient->add<CURLOPT_URL>((s.getUrl() + remotePath).data());
    performGet(target);
}

// template instantiations
template void MarcRestClient::upload(string remotePath, AbstractStorage &body, size_t start, size_t count);
template void MarcRestClient::download(string remotePath, AbstractStorage& target);
template void MarcRestClient::performGet(AbstractStorage &target);
