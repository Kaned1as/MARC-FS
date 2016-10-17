#include "api.h"

#include "curl_easy.h"

const std::string AUTH_ENDPOINT = "https://auth.mail.ru";

API::API()
{

}

bool API::login(const Account &acc)
{
    if (acc.login.empty())
        return false;

    if (acc.password.empty())
        return false;

    curl::curl_easy client;
    client.add<CURLOPT_URL>()
}
