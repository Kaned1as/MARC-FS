#ifndef OBJECT_POOL_H
#define OBJECT_POOL_H

#include <list>
#include <stack>
#include <mutex>
#include <condition_variable>

#include <iostream>

template<typename T>
class ObjectPool
{
public:
    ObjectPool(size_t num)
    {
        for (size_t i = 0; i < num; ++i) {
            objects.emplace_back(std::shared_ptr<T>(new T(), ExternalDeleter(this)));
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
            std::cout << "Acquiring API object..." << std::endl;
            condition.wait(lock, [this]{ return !objects.empty(); });
            auto tmp = std::move(objects.front());
            objects.pop_front();
            return tmp;
        }
    }

    void add(std::shared_ptr<T> obj) {
        {
            std::unique_lock<std::mutex> lock(acquire_mutex);
            std::cout << "Returning API object to the pool..." << std::endl;
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
                pool->add(std::shared_ptr<T>(ptr, *this));
                return;
        }

    private:
        ObjectPool<T>* pool;
    };


    std::condition_variable condition;
    std::mutex acquire_mutex;

    std::list<std::shared_ptr<T>> objects;
};

#endif // OBJECT_POOL_H
