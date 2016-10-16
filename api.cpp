#include "api.h"

API::API()
{

}

bool API::login(const Account &acc)
{
    if (acc.login.empty())
        return false;

    if (acc.password.empty())
        return false;
}
