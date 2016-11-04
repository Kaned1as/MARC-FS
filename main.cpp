#include <iostream>

#include "account.h"
#include "api.h"

using namespace std;



int main(int argc, char *argv[])
{
    Account acc;
    acc.setLogin("kairllur@mail.ru");
    acc.setPassword("REDACTED");

    API api;
    api.login(acc);
    api.upload("", "");

    return 0;
}
