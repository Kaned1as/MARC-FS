#ifndef UTILS_H
#define UTILS_H

#include <curl_info.h>

#include "object_pool.h"

/**
 * @brief The SpaceInfo struct - just a simple wrapper for API space call
 *        answer.
 *
 * @see API::df
 */
struct SpaceInfo
{
    size_t totalMiB = 0;
    uint32_t usedMiB = 0;
};

struct MailApiException : public std::exception {
    MailApiException(std::string reason) : reason(reason) {}

    std::string reason;

    virtual const char *what() const noexcept override;
};

void dump(const char *text, FILE *stream, unsigned char *ptr, size_t size, char nohex);
int trace_post(CURL *handle, curl_infotype type, char *data, size_t size, void *userp);

#endif // UTILS_H
