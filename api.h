#ifndef API_H
#define API_H

#include "account.h"

#include "curl_easy.h"

#include <memory>

namespace curl {
class curl_easy;
}

class API
{
public:
    API();

    /**
     * @brief API::login Sends auth info and initializes this API object on successful login.
     * @param acc account to auth with
     * @return true if authenticated successfully, false otherwise.
     */
    bool login(const Account& acc);
private:
    std::unique_ptr<curl::curl_easy> m_client;
};

#endif // API_H
