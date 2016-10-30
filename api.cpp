#include "curl_header.h"
#include "curl_ios.h"

#include "api.h"

#include <json/json.h>

#define NV_PAIR(name, value) curl_pair<CURLformoption, std::string>(CURLFORM_COPYNAME, name), \
                             curl_pair<CURLformoption, std::string>(CURLFORM_COPYCONTENTS, value.c_str())

static const std::string SAFE_USER_AGENT = "Mozilla / 5.0(Windows; U; Windows NT 5.1; en - US; rv: 1.9.0.1) Gecko / 2008070208 Firefox / 3.0.1";

static const std::string AUTH_DOMAIN = "https://auth.mail.ru";
static const std::string CLOUD_DOMAIN = "https://cloud.mail.ru";

static const std::string AUTH_ENDPOINT = AUTH_DOMAIN + "/cgi-bin/auth";
static const std::string SCLD_COOKIE_ENDPOINT = AUTH_DOMAIN + "/sdc";
static const std::string SCLD_TOKEN_ENDPOINT = CLOUD_DOMAIN + "/api/v2/tokens/csrf";

using curl::curl_easy;
using curl::curl_pair;
using curl::curl_header;
using curl::curl_form;
using curl::curl_cookie;
using curl::curl_ios;

API::API()
    : m_client(std::make_unique<curl::curl_easy>()),
      m_cookies(*m_client)
{
    m_cookies.set_file(""); // init cookie engine
}

bool API::login(const Account &acc)
{
    if (acc.login.empty())
        return false;

    if (acc.password.empty())
        return false;

    if (!authenticate(acc))
        return false;

    if (!obtainCloudCookie())
        return false;

    if (!obtainAuthToken())
        return false;

    return true;
}

/**
 * @brief API::authenticate - retrieves initial authentication cookies
 * @return true if cookies were successfully set, false otherwise
 */
bool API::authenticate(const Account &acc)
{
    // Login={0}&Domain={1}&Password={2}

    curl_form form;
    form.add(NV_PAIR("Login", acc.login));
    form.add(NV_PAIR("Password", acc.password));
    form.add(NV_PAIR("Domain", std::string("mail.ru")));

    m_client->add<CURLOPT_URL>(AUTH_ENDPOINT.data());
    m_client->add<CURLOPT_HTTPPOST>(form.get());
    try {
        m_client->perform();
    } catch (curl::curl_easy_exception error) {
        curl::curlcpp_traceback errors = error.get_traceback();
        error.print_traceback();
        return false;
    }
    int64_t ret = m_client->get_info<CURLINFO_RESPONSE_CODE>().get();
    if (ret != 302 && ret != 200) // OK or redirect
        return false;

    if (m_cookies.get().empty()) // no cookies received, halt
        return false;

    m_client->reset();
    return true;
}

/**
 * @brief API::obtainCloudCookie - retrieves basic cloud cookie that is needed for API exchange
 * @return true if cookie was successfully obtained, false otherwise
 */
bool API::obtainCloudCookie()
{
    curl_form form;
    form.add(NV_PAIR("from", std::string(CLOUD_DOMAIN + "/home")));

    size_t cookies_size = m_cookies.get().size();

    m_client->add<CURLOPT_URL>(SCLD_COOKIE_ENDPOINT.data());
    m_client->add<CURLOPT_HTTPPOST>(form.get());
    m_client->add<CURLOPT_FOLLOWLOCATION>(1L);
    try {
        m_client->perform();
    } catch (curl::curl_easy_exception error) {
        curl::curlcpp_traceback errors = error.get_traceback();
        error.print_traceback();
        return false;
    }
    int64_t ret = m_client->get_info<CURLINFO_RESPONSE_CODE>().get();
    if (ret != 302 && ret != 200) // OK or redirect
        return false;

    if (m_cookies.get().size() <= cookies_size) // didn't get any new cookies
        return false;

    m_client->reset();
    return true;
}

/**
 * @brief API::obtainAuthToken - retrieve auth token. This is the first step in Mail.ru cloud API exchange
 * @return true if token was obtained, false otherwise
 */
bool API::obtainAuthToken()
{
    curl_header header;
    header.add("Accept: application/json");

    std::ostringstream stream;
    curl_ios<std::ostringstream> writer(stream);
    
    m_client->add<CURLOPT_USERAGENT>(SAFE_USER_AGENT.data()); // 403 without this
    m_client->add<CURLOPT_HTTPHEADER>(header.get());
    m_client->add<CURLOPT_URL>(SCLD_TOKEN_ENDPOINT.c_str());
    m_client->add<CURLOPT_WRITEFUNCTION>(writer.get_function());
    m_client->add<CURLOPT_WRITEDATA>(writer.get_stream());
    try {
        m_client->perform();
    } catch (curl::curl_easy_exception error) {
        curl::curlcpp_traceback errors = error.get_traceback();
        error.print_traceback();
        return false;
    }

    int64_t ret = m_client->get_info<CURLINFO_RESPONSE_CODE>().get();
    if (ret != 302 && ret != 200) // OK or redirect
        return false;

    using Json::Value;

    Value response;
    Json::Reader reader;

    if (!reader.parse(stream.str(), response)) // invalid JSON (shouldn't happen)
        return false;

    if (response["body"] != Value() && response["body"]["token"] != Value()) {
        m_token = response["body"]["token"].asString();
        return true;
    }

    return false;
}
