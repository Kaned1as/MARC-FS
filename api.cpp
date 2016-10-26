#include "curl_header.h"

#include "api.h"

#define NV_PAIR(name, value) curl_pair<CURLformoption, std::string>(CURLFORM_COPYNAME, name), \
                             curl_pair<CURLformoption, std::string>(CURLFORM_COPYCONTENTS, value.c_str())

static const char* AUTH_ENDPOINT = "https://auth.mail.ru/cgi-bin/auth";
static const char* SCLD_ENDPOINT = "https://auth.mail.ru/sdc";

using curl::curl_easy;
using curl::curl_pair;
using curl::curl_header;
using curl::curl_form;
using curl::curl_cookie;

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

    // Login={0}&Domain={1}&Password={2}

    curl_form form;
    form.add(NV_PAIR("Login", acc.login));
    form.add(NV_PAIR("Password", acc.password));
    form.add(NV_PAIR("Domain", std::string("mail.ru")));

    m_client->add<CURLOPT_URL>(AUTH_ENDPOINT);
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
    return obtainCloudCookie();
}

bool API::obtainCloudCookie()
{
    curl_form form;
    form.add(NV_PAIR("from", std::string("https://cloud.mail.ru/home")));

    m_client->add<CURLOPT_URL>(SCLD_ENDPOINT);
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

    m_client->reset();
    return true;
}
