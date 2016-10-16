#ifndef API_H
#define API_H

#include "account.h"

class API
{
public:
    API();

    bool login(const Account& acc);
};

#endif // API_H
