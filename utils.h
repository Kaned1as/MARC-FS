/*****************************************************************************
 *
 * Copyright (c) 2017, Oleg `Kanedias` Chernovskiy
 *
 * This file is part of MARC-FS.
 *
 * MARC-FS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MARC-FS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MARC-FS.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef UTILS_H
#define UTILS_H

#include <functional>
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

/**
 * @brief The MailApiException struct - exception used to indicate that Cloud REST API
 *        intercommunication failed for some reason.
 * @see MarcRestClient
 */
struct MailApiException : public std::exception {
    MailApiException(std::string reason, int64_t responseCode)
        : reason(reason),
          httpResponseCode(responseCode)
    {
    }
    MailApiException(std::string reason)
        : reason(reason),
          httpResponseCode(0)
    {
    }

    std::string reason;
    int64_t httpResponseCode;

    virtual const char *what() const noexcept override;
    int64_t getResponseCode() const {
        return httpResponseCode;
    }
};

/**
 * @brief The ScopeGuard class - `defer`/`finally` pattern implementation from Go/Java
 *
 * @note taken from http://stackoverflow.com/questions/10270328/the-simplest-and-neatest-c11-scopeguard
 *
 * Used to do essential cleanup in case of throw or return from the function.
 * Will always run the function passed after scope where it resides is closed.
 */
class ScopeGuard {
public:
    template<class Callable>
    ScopeGuard(Callable && scopeCloseFunc) : func(std::forward<Callable>(scopeCloseFunc)) {}

    ScopeGuard(ScopeGuard && other) : func(std::move(other.func)) {
        other.func = nullptr;
    }

    ~ScopeGuard() {
        if(func) func(); // must not throw
    }

    void dismiss() noexcept {
        func = nullptr;
    }

    ScopeGuard(const ScopeGuard&) = delete;
    void operator = (const ScopeGuard&) = delete;

private:
    std::function<void()> func;
};

/**
 * RAII-style helper class to wrap file-based locking transparently. On creation the instance obtains
 * the lock of contained file, on destruction the lock is released.
 *
 * @see MruData::getNode
 * @see fuse_hooks.cpp
 */
template <typename T>
struct LockHolder {
    LockHolder()
        : content(nullptr), scopeLock()
    {
    }

    explicit LockHolder(T* node)
        : content(node), scopeLock(node->getMutex())
    {
    }

    T* operator->() const noexcept {
      return content;
    }

    operator bool() const {
        return content;
    }

private:
    T* content;
    std::unique_lock<std::mutex> scopeLock;
};

// C-style functions for cURL
void dump(const char *text, FILE *stream, unsigned char *ptr, size_t size, char nohex);
int trace_post(CURL *handle, curl_infotype type, char *data, size_t size, void *userp);

#endif // UTILS_H
