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

#ifndef OBJECT_POOL_H
#define OBJECT_POOL_H

#include <list>
#include <stack>
#include <mutex>
#include <condition_variable>

#include <iostream>

/**
 * Shared-pointer based object pool that readds objects that are no longer used.
 * Thread-safe (I hope).
 */
template<typename T>
class ObjectPool
{
public:
    ~ObjectPool() {
        shuttingDown = true;
    }

    void populate(T& obj, size_t number) {
        for (size_t i = 0; i < number; ++i) {
            objects.emplace_back(std::shared_ptr<T>(new T(obj), ExternalDeleter(this)));
        }
    }

    template<typename F, typename ...Args>
    void initialize(F&& functor, Args... args) {
        for (int i = 0; i < objects.size(); ++i) {
            auto obj = acquire();
            (obj.get()->*functor)(args...);
        }
    }

    std::shared_ptr<T> acquire() {
        {
            std::unique_lock<std::mutex> lock(acquire_mutex);
            condition.wait(lock, [this]{ return !objects.empty(); });
            auto tmp = std::move(objects.front());
            objects.pop_front();
            return tmp;
        }
    }

    void add(std::shared_ptr<T> obj) {
        {
            std::unique_lock<std::mutex> lock(acquire_mutex);
            objects.emplace_back(std::move(obj));
        }
        condition.notify_one();
    }

private:
    // deleter that returns objects to the pool
    struct ExternalDeleter {
        explicit ExternalDeleter(ObjectPool<T>* pool)
        : pool(pool) {}

        void operator()(T* ptr) {
            if (pool->shuttingDown)
                return;

            pool->add(std::shared_ptr<T>(ptr, *this));
            return;
        }

    private:
        ObjectPool<T>* pool;
    };

    std::condition_variable condition;
    std::mutex acquire_mutex;

    bool shuttingDown = false;
    std::list<std::shared_ptr<T>> objects;
};

#endif // OBJECT_POOL_H
