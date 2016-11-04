#ifndef API_H
#define API_H

#include "account.h"
#include "api_shard.h"

#include "curl_easy.h"
#include "curl_cookie.h"

#include <memory>

namespace curl {
class curl_easy;
}

class API
{
    using Params = std::map<std::string, std::string>;

public:
    API();

    /**
     * @brief API::login Sends auth info and initializes this API object on successful login.
     * @param acc account to auth with
     * @return true if authenticated successfully, false otherwise.
     */
    bool login(const Account& acc);
    void upload(std::string path, std::string remote_path);
private:
    bool authenticate(const Account &acc);
    bool obtainCloudCookie();
    bool obtainAuthToken();

    Shard obtainShard();

    std::string paramString(Params const &params);

    std::string m_token;

    std::unique_ptr<curl::curl_easy> m_client;
    curl::curl_cookie m_cookies;
};

#endif // API_H
