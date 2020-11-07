/*****************************************************************************
 *
 * Copyright (c) 2018-2020, Oleg `Kanedias` Chernovskiy
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

#include <ctime>
#include <json/json.h>

#include <iterator>
#include <iomanip>
#include <memory>
#include <algorithm>
#include <string>

#include "curl_header.h"

#include "marc_rest_client.h"
#include "abstract_storage.h"

#define NV_PAIR(name, value) curl_pair<CURLformoption, std::string>(CURLFORM_COPYNAME, name), \
                             curl_pair<CURLformoption, std::string>(CURLFORM_COPYCONTENTS, value.c_str())

using namespace curl;

static const std::string SAFE_USER_AGENT = "Mozilla/5.0 (Windows NT 10.0; rv:78.0) Gecko/20100101 Firefox/78.0";

static const std::string MAIN_DOMAIN = "https://mail.ru";
static const std::string AUTH_DOMAIN = "https://auth.mail.ru";
static const std::string CLOUD_DOMAIN = "https://cloud.mail.ru";

static const std::string AUTH_ENDPOINT = AUTH_DOMAIN + "/jsapi/auth";
static const std::string SCLD_COOKIE_ENDPOINT = AUTH_DOMAIN + "/sdc";

static const std::string SCLD_SHARD_ENDPOINT = CLOUD_DOMAIN + "/api/v2/dispatcher";

static const std::string SCLD_FOLDER_ENDPOINT = CLOUD_DOMAIN + "/api/v2/folder";
static const std::string SCLD_FILE_ENDPOINT = CLOUD_DOMAIN + "/api/v2/file";
static const std::string SCLD_SPACE_ENDPOINT = CLOUD_DOMAIN + "/api/v2/user/space";

static const std::string SCLD_ADDFILE_ENDPOINT = SCLD_FILE_ENDPOINT + "/add";
static const std::string SCLD_PUBLISHFILE_ENDPOINT = SCLD_FILE_ENDPOINT + "/publish";
static const std::string SCLD_REMOVEFILE_ENDPOINT = SCLD_FILE_ENDPOINT + "/remove";
static const std::string SCLD_RENAMEFILE_ENDPOINT = SCLD_FILE_ENDPOINT + "/rename";
static const std::string SCLD_MOVEFILE_ENDPOINT = SCLD_FILE_ENDPOINT + "/move";
static const std::string SCLD_ADDFOLDER_ENDPOINT = SCLD_FOLDER_ENDPOINT + "/add";

const std::string SCLD_PUBLICLINK_ENDPOINT = CLOUD_DOMAIN + "/public";

static std::string toPadded40Hex(const std::string& s) {
    if (s.length() >= 40) {
        throw MailApiException("String is too long");
    }

    // fill the stream
    std::ostringstream ret;
    for (std::string::size_type i = 0; i < s.length(); ++i)
        ret << std::hex << std::setfill('0') << std::setw(2) << std::uppercase << (uint32_t) s[i];

    size_t remaining = 40 - ret.tellp();
    if (remaining > 0) {
        ret << std::setw(remaining) << 0;
    }

    return ret.str();
}

MarcRestClient::MarcRestClient()
    : restClient(std::make_unique<curl::curl_easy>()),
      cookieStore(*restClient) {
    cookieStore.set_file("");   // init cookie engine
    restClient->reset();        // reset debug->std:cout function
}


MarcRestClient::MarcRestClient(MarcRestClient &toCopy)
    : restClient(std::make_unique<curl::curl_easy>(*toCopy.restClient.get())), // copy easy handle
      cookieStore(*restClient),                 // cokie_store is not copyable, init in body
      proxyUrl(toCopy.proxyUrl),
      maxDownloadRate(toCopy.maxDownloadRate),
      maxUploadRate(toCopy.maxUploadRate),
      authAccount(toCopy.authAccount),                      // copy account from other one
      csrfToken(toCopy.csrfToken),
      actToken(toCopy.actToken) {               // copy auth token from other one
    for (const auto &c : toCopy.cookieStore.get()) {
        restClient->add<CURLOPT_COOKIELIST>(c.data());
    }
    cookieStore.set_file("");                   // init cookie engine
}

void MarcRestClient::setProxy(std::string proxyUrl) {
    this->proxyUrl = proxyUrl;
}

void MarcRestClient::setMaxDownloadRate(uint64_t rate) {
    this->maxDownloadRate = rate;
}

void MarcRestClient::setMaxUploadRate(uint64_t rate) {
    this->maxUploadRate = rate;
}

std::string MarcRestClient::paramString(Params const &params) {
    if (params.empty())
        return "";

    std::vector<std::string> result;
    result.reserve(params.size());
    for (auto it = params.cbegin(); it != params.end(); ++it) {
        std::string name = it->first, value = it->second;
        if (name.empty())
            continue;

        restClient->escape(name);
        restClient->escape(value);
        std::string argument = value.empty() ? name : name + "=" + value;
        result.push_back(argument);
    }

    std::stringstream s;
    copy(result.cbegin(), result.cend(), std::ostream_iterator<std::string>(s, "&"));
    return s.str();
}

std::string MarcRestClient::performAction(curl::curl_header *forced_headers) {
    std::ostringstream stream;
    curl_ios<std::ostringstream> writer(stream);

    curl_header header;
    if (forced_headers) {
        // we have forced headers, override defaults with them
        header = *forced_headers;
    } else {
        // no forced headers, populate with defaults
        header.add("Accept: */*");
        header.add("Origin: " + CLOUD_DOMAIN);
        header.add("Referer: " + CLOUD_DOMAIN);

        if (!csrfToken.empty()) {
            header.add("X-CSRF-Token: " + csrfToken);
        }
    }

    ScopeGuard resetter = [&] { restClient->reset(); };
    if (!this->proxyUrl.empty())
        restClient->add<CURLOPT_PROXY>(this->proxyUrl.data());
    if (this->maxUploadRate)
        restClient->add<CURLOPT_MAX_SEND_SPEED_LARGE>(this->maxUploadRate);

    restClient->add<CURLOPT_CONNECTTIMEOUT>(10L);
    restClient->add<CURLOPT_TCP_KEEPALIVE>(1L);
    restClient->add<CURLOPT_FOLLOWLOCATION>(1L);
    restClient->add<CURLOPT_TIMEOUT>(0L);
    restClient->add<CURLOPT_NOSIGNAL>(1L);
    restClient->add<CURLOPT_HTTPHEADER>(header.get());
    restClient->add<CURLOPT_USERAGENT>(SAFE_USER_AGENT.data());  // 403 without this
    restClient->add<CURLOPT_VERBOSE>(verbose);
    restClient->add<CURLOPT_DEBUGFUNCTION>(trace_post);

    restClient->add<CURLOPT_WRITEFUNCTION>(writer.get_function());
    restClient->add<CURLOPT_WRITEDATA>(writer.get_stream());
    try {
        restClient->perform();
    } catch (curl::curl_easy_exception &error) {
        error.print_traceback();
        throw MailApiException("Couldn't perform request!");
    }
    int64_t ret = restClient->get_info<CURLINFO_RESPONSE_CODE>().get();
    if (ret != 302 && ret != 200 && ret != 201) {  // OK or redirect
        throw MailApiException("Non-success return code! Error message body: " + stream.str(), ret);
    }

    return stream.str();
}

void MarcRestClient::performGet(AbstractStorage &target) {
    curl_header header;
    header.add("Accept: */*");
    header.add("Origin: " + CLOUD_DOMAIN);

    ScopeGuard resetter = [&] { restClient->reset(); };
    if (!this->proxyUrl.empty())
        restClient->add<CURLOPT_PROXY>(this->proxyUrl.data());
    if (this->maxDownloadRate)
        restClient->add<CURLOPT_MAX_RECV_SPEED_LARGE>(this->maxDownloadRate);
    restClient->add<CURLOPT_CONNECTTIMEOUT>(10L);
    restClient->add<CURLOPT_TCP_KEEPALIVE>(1L);
    restClient->add<CURLOPT_FOLLOWLOCATION>(1L);
    restClient->add<CURLOPT_TIMEOUT>(0L);
    restClient->add<CURLOPT_NOSIGNAL>(1L);
    restClient->add<CURLOPT_HTTPHEADER>(header.get());
    restClient->add<CURLOPT_USERAGENT>(SAFE_USER_AGENT.data()); // 403 without this
    restClient->add<CURLOPT_VERBOSE>(verbose);
    restClient->add<CURLOPT_DEBUGFUNCTION>(trace_post);

    restClient->add<CURLOPT_WRITEDATA>(&target);
    restClient->add<CURLOPT_WRITEFUNCTION>([](void *contents, size_t size, size_t nmemb, void *userp) {
        auto result = static_cast<AbstractStorage *>(userp);
        char *bytes = static_cast<char *>(contents);
        const size_t realsize = size * nmemb;
        result->append(bytes, realsize);
        return realsize;
    });

    try {
        restClient->perform();
    } catch (curl::curl_easy_exception &error) {
        curl::curlcpp_traceback errors = error.get_traceback();
        error.print_traceback();
        throw MailApiException("Couldn't perform request!");
    }
    int64_t ret = restClient->get_info<CURLINFO_RESPONSE_CODE>().get();
    if (ret != 302 && ret != 200) { // OK or redirect
        if (target.empty())
            throw MailApiException("Non-success return code!", ret);

        std::string body = target.readFully();
        throw MailApiException(std::string("Non-success return code! Body:") + body, ret);
    }
}

bool MarcRestClient::login(const Account &acc) {
    if (acc.login.empty())
        throw MailApiException("Login not specified!");

    if (acc.password.empty())
        throw MailApiException("Password not specified!");

    authAccount = acc;

    openMainPage();
    authenticate();
    openCloudPage();

    return true;
}

void MarcRestClient::openMainPage() {
    size_t cookiesSize = cookieStore.get().size();

    restClient->add<CURLOPT_URL>(MAIN_DOMAIN.data());
    performAction();

    auto savedCookies = cookieStore.get();
    if (savedCookies.size() <= cookiesSize)  // no more cookies received, halt
        throw MailApiException("Failed to retrieve cookies from main mail.ru page");

    auto tokenItr = std::find_if(savedCookies.begin(), savedCookies.end(), [](std::string &arg) { return arg.find("act") != std::string::npos; });
    if (tokenItr == savedCookies.end())
        throw MailApiException("No token cookie retrieved from main mail.ru page");

    // extract act token
    size_t tokenStart = tokenItr->find_last_of('\t');
    actToken = tokenItr->substr(tokenStart + 1, tokenItr->size() - tokenStart);
}

void MarcRestClient::openCloudPage() {
    size_t cookiesSize = cookieStore.get().size();

    restClient->add<CURLOPT_URL>(CLOUD_DOMAIN.data());
    std::string html = performAction();

    if (cookieStore.get().size() <= cookiesSize) // didn't get any new cookies
        throw MailApiException("Failed to obtain cloud cookie, did you sign up to the cloud?");

    size_t csrfTagStartPos = html.find("\"csrf\": \"");
    if (csrfTagStartPos == std::string::npos) {
        throw MailApiException("Couldn't find CSRF token in cloud page");
    }

    size_t csrfQuoteStartPos = csrfTagStartPos + 9;
    size_t csrfQuoteEndPos = html.find("\"", csrfQuoteStartPos);
    csrfToken = html.substr(csrfQuoteStartPos, csrfQuoteEndPos - csrfQuoteStartPos);
}

void MarcRestClient::create(std::string remotePath) {
    std::string filename = remotePath.substr(remotePath.find_last_of("/\\") + 1);
    std::string parentDir = remotePath.substr(0, remotePath.find_last_of("/\\") + 1);

    // add zero file, special hash
    addUploadedFile(filename, parentDir, "0000000000000000000000000000000000000000", 0);
}

void MarcRestClient::authenticate() {
    size_t cookiesSize = cookieStore.get().size();

    curl_form form;
    form.add(NV_PAIR("login", authAccount.login));
    form.add(NV_PAIR("password", authAccount.password));
    form.add(NV_PAIR("project", std::string("e.mail.ru")));
    form.add(NV_PAIR("saveauth", std::string("1")));
    form.add(NV_PAIR("token", actToken));

    restClient->add<CURLOPT_URL>(AUTH_ENDPOINT.data());
    restClient->add<CURLOPT_HTTPPOST>(form.get());

    curl_header force_main_domain;
    force_main_domain.add("Accept: */*");
    force_main_domain.add("Origin: " + MAIN_DOMAIN);
    force_main_domain.add("Referer: " + MAIN_DOMAIN);

    performAction(&force_main_domain);

    if (cookieStore.get().size() <= cookiesSize)  // no cookies received, halt
        throw MailApiException("Failed to authenticate with " + authAccount.login + " credentials");
}

Shard MarcRestClient::obtainShard(Shard::ShardType type) {
    using Json::Value;

    restClient->add<CURLOPT_URL>(SCLD_SHARD_ENDPOINT.data());
    std::string answer = performAction();

    Value response;
    std::istringstream(answer) >> response;

    if (response["body"] != Value()) {
        return Shard(response["body"][Shard::asString(type)]);
    }

    throw MailApiException("Non-Shard json received: " + answer);
}

void MarcRestClient::addUploadedFile(std::string name, std::string remoteDir, std::string hash, size_t size) {
    std::string postFields = paramString({
        {"hash", hash},
        {"size", std::to_string(size)},
        {"home", remoteDir + name},
        {"conflict", "rewrite"},  // also: rename, strict
        {"api", "2"}
    });

    restClient->add<CURLOPT_URL>(SCLD_ADDFILE_ENDPOINT.data());
    restClient->add<CURLOPT_POSTFIELDS>(postFields.data());
    performAction();
}

void MarcRestClient::move(std::string whatToMove, std::string whereToMove) {
    std::string postFields = paramString({
        {"api", "2"},
        {"conflict", "rewrite"},  // also: rename, strict
        {"folder", whereToMove},
        {"home", whatToMove}
    });

    restClient->add<CURLOPT_URL>(SCLD_MOVEFILE_ENDPOINT.data());
    restClient->add<CURLOPT_POSTFIELDS>(postFields.data());
    performAction();
}

void MarcRestClient::remove(std::string remotePath) {
    std::string postFields = paramString({
        {"api", "2"},
        {"home", remotePath}
    });

    restClient->add<CURLOPT_URL>(SCLD_REMOVEFILE_ENDPOINT.data());
    restClient->add<CURLOPT_POSTFIELDS>(postFields.data());
    performAction();
}

SpaceInfo MarcRestClient::df() {
    using Json::Value;

    std::string getFields = paramString({
        {"api", "2"}
    });

    restClient->add<CURLOPT_URL>((SCLD_SPACE_ENDPOINT + "?" + getFields).data());
    std::string answer = performAction();

    SpaceInfo result;
    Value response;
    std::istringstream(answer) >> response;

    // if `total` is there, `used` will definitely be...
    if (response["body"] == Value() || response["body"]["bytes_total"] == Value())
        throw MailApiException("Non-well formed JSON df response!");

    Value &total = response["body"]["bytes_total"];
    Value &used = response["body"]["bytes_used"];

    result.total = total.asUInt64();
    result.used = used.asUInt64();
    return result;
}

void MarcRestClient::rename(std::string oldRemotePath, std::string newRemotePath) {
    std::string oldFilename = oldRemotePath.substr(oldRemotePath.find_last_of("/\\") + 1);
    std::string oldParentDir = oldRemotePath.substr(0, oldRemotePath.find_last_of("/\\") + 1);

    std::string newFilename = newRemotePath.substr(newRemotePath.find_last_of("/\\") + 1);
    std::string newParentDir = newRemotePath.substr(0, newRemotePath.find_last_of("/\\") + 1);

    std::string postFields = paramString({
        {"api", "2"},
        {"conflict", "rewrite"}, // also: rename, strict
        {"home", oldRemotePath},
        {"name", newFilename}
    });

    restClient->add<CURLOPT_URL>(SCLD_RENAMEFILE_ENDPOINT.data());
    restClient->add<CURLOPT_POSTFIELDS>(postFields.data());
    performAction();

    // FIXME: think about version that doesn't rewrite file with name == newFilename
    // placed in the old dir.

    if (oldParentDir != newParentDir) {
        move(oldParentDir + newFilename, newParentDir);
    }
}

std::string MarcRestClient::share(std::string remotePath) {
    using Json::Value;

    std::string postFields = paramString({
        {"api", "2"},
        {"home", remotePath}
    });

    restClient->add<CURLOPT_URL>(SCLD_PUBLISHFILE_ENDPOINT.data());
    restClient->add<CURLOPT_POSTFIELDS>(postFields.data());
    std::string answer = performAction();

    Value response;
    std::istringstream(answer) >> response;

    if (response["body"] == Value())
        throw MailApiException("Non-well formed JSON share response!");

    return response["body"].asString();
}

struct ReadData {
    /*const*/ AbstractStorage * const content;  // content to read from
    off_t offset;  // current offset of read
    off_t count;   // maximum offset - can be lower than content.size()
};

void MarcRestClient::upload(std::string remotePath, AbstractStorage &body, off_t start, off_t count) {
    if (body.empty()) {
        // zero size upload requested, skip upload part completely
        create(remotePath);
        return;
    }

    std::string filename = remotePath.substr(remotePath.find_last_of("/\\") + 1);
    std::string parentDir = remotePath.substr(0, remotePath.find_last_of("/\\") + 1);

    if (body.size() <= 20) {
        // Mail.ru Cloud has special handling for files that are no more than 20 bytes in size
        std::string content = body.readFully();
        addUploadedFile(filename, parentDir, toPadded40Hex(content), body.size());
        return;
    }

    Shard s = obtainShard(Shard::ShardType::UPLOAD);
    std::string uploadUrl = s.getUrl() + "?" + paramString({{"cloud_domain", "2"}, {"x-email", authAccount.login}});

    // fileupload part
    curl_form nameForm;
    off_t realSize = std::min(static_cast<off_t>(body.size()) - start, count);  // size to transfer
    ReadData ptr {&body, start, std::min(static_cast<off_t>(body.size()), start + count)};

    restClient->add<CURLOPT_URL>(uploadUrl.data());
    restClient->add<CURLOPT_UPLOAD>(1L);
    //restClient->add<CURLOPT_INFILESIZE_LARGE>(realSize);
    restClient->add<CURLOPT_READDATA>(&ptr);
    restClient->add<CURLOPT_READFUNCTION>([](void *contents, size_t size, size_t nmemb, void *userp) {
        auto source = static_cast<ReadData *>(userp);
        auto target = static_cast<char *>(contents);
        const size_t requested = size * nmemb;
        const size_t available = source->count - source->offset;
        const size_t transferred = std::min(requested, available);
        source->content->read(target, transferred, source->offset);
        source->offset += transferred;
        return transferred;
    });
    std::string hash = performAction();

    addUploadedFile(filename, parentDir, hash, realSize);
}

void MarcRestClient::mkdir(std::string remotePath) {
    std::string postFields = paramString({
        {"api", "2"},
        {"conflict", "rewrite"},  // also: rename, strict
        {"home", remotePath}
    });

    restClient->add<CURLOPT_URL>(SCLD_ADDFOLDER_ENDPOINT.data());
    restClient->add<CURLOPT_POSTFIELDS>(postFields.data());
    performAction();
}

std::vector<CloudFile> MarcRestClient::ls(std::string remotePath) {
    using Json::Value;

    std::string getFields = paramString({
        {"api", "2"},
        {"offset", "0" }, // 100500 files in folder - who'd dare for more?
        {"limit", "100500" }, // 100500 files in folder - who'd dare for more?
        {"home", remotePath}
    });

    restClient->add<CURLOPT_URL>((SCLD_FOLDER_ENDPOINT + "?" + getFields).data());
    std::string answer = performAction();

    std::vector<CloudFile> results;
    Value response;
    std::istringstream(answer) >> response;

    if (response["body"] == Value() || response["body"]["list"] == Value())
        throw MailApiException("Non-well formed JSON ls response!");

    Value &list = response["body"]["list"];
    for (const Value &entry : list) {
        results.push_back(CloudFile(entry));
    }

    return results;
}

void MarcRestClient::download(std::string remotePath, AbstractStorage &target) {
    Shard s = obtainShard(Shard::ShardType::GET);
    restClient->escape(remotePath);
    restClient->add<CURLOPT_URL>((s.getUrl() + remotePath).data());
    performGet(target);
}
