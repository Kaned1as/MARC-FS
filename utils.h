#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <curl_info.h>

#include "thread_pool.h"
#include "object_pool.h"
#include "api.h"

struct MruData {

    MruData() : apiPool(1) {

    }

    std::map<std::string, std::vector<char>> cached;
    std::map<std::string, bool> dirty;
    std::map<std::string, API::Pipe> writes;
    ObjectPool<API> apiPool;
};

struct MailApiException : public std::exception {
    MailApiException(std::string reason) : reason(reason) {}

    std::string reason;

    virtual const char *what() const noexcept override;
};

void dump(const char *text, FILE *stream, unsigned char *ptr, size_t size, char nohex);
int trace_post(CURL *handle, curl_infotype type, char *data, size_t size, void *userp);

#endif // UTILS_H
