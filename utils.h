#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <curl_info.h>

struct MailApiException : public std::exception {
    MailApiException(std::string reason) : reason(reason) {}

    std::string reason;

    virtual const char *what() const noexcept override;
};

void dump(const char *text, FILE *stream, unsigned char *ptr, size_t size, char nohex);
int trace_post(CURL *handle, curl_infotype type, char *data, size_t size, void *userp);

#endif // UTILS_H
