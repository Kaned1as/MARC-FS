#include "account.h"

Account::Account()
{

}

std::string Account::getLogin() const
{
    return login;
}

void Account::setLogin(const std::string &value)
{
    login = value;
}

std::string Account::getPassword() const
{
    return password;
}

void Account::setPassword(const std::string &value)
{
    password = value;
}
