#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <string>

class Account
{
public:
    Account();

    std::string getLogin() const;
    void setLogin(const std::string &value);

    std::string getPassword() const;
    void setPassword(const std::string &value);

private:
    std::string login;
    std::string password;

    friend class MarcRestClient;
};

#endif // ACCOUNT_H
