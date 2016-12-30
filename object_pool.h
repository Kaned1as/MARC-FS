#ifndef OBJECT_POOL_H
#define OBJECT_POOL_H

#include <list>
#include <stack>
#include <mutex>
#include <condition_variable>

template<typename T>
class ObjectPool
{
    using PtrType = std::unique_ptr<T, std::function<void(T*)>>;

public:
    ObjectPool(size_t num)
    {
        for (int i = 0; i < num; ++i) {
            objects.emplace_back(std::unique_ptr<T>(new T()));
        }
    }

    template<typename F, typename ...Args>
    void initialize(F&& functor, Args... args) {
        for (int i = 0; i < objects.size(); ++i) {
            PtrType obj = acquire();
            (obj.get()->*functor)(args...);
        }
    }

    PtrType acquire() {
        {
            std::unique_lock<std::mutex> lock(acquire_mutex);
            condition.wait(lock, [this]{ return !objects.empty(); });
            PtrType tmp(objects.front().release(), ExternalDeleter(this));
            objects.pop_front();
            return std::move(tmp);
        }
    }

    void add(std::unique_ptr<T> obj) {
        {
            std::unique_lock<std::mutex> lock(acquire_mutex);
            objects.emplace_back(std::move(obj));
            condition.notify_one();
        }
    }

private:
    // deleter that returns objects to the pool
    struct ExternalDeleter {
        explicit ExternalDeleter(ObjectPool<T>* pool)
        : pool(pool) {}

        void operator()(T* ptr) {
                pool->add(std::unique_ptr<T>{ptr});
                return;
        }

    private:
        ObjectPool<T>* pool;
    };


    std::condition_variable condition;
    std::mutex acquire_mutex;

    std::list<std::unique_ptr<T>> objects;
};

#endif // OBJECT_POOL_H
