#ifndef BLOCKING_QUEUE_H
#define BLOCKING_QUEUE_H


#include <mutex>
#include <condition_variable>
#include <deque>
#include <vector>
#include <algorithm>

template <typename T, typename K = std::vector<T>>
class BlockingQueue
{
public:
    void push(T const& value) {
        {
            std::unique_lock<std::mutex> lock(mutex);
            queue.push_front(value);
        }
        condition.notify_one();
    }

    /**
     * @brief push - overloaded to accept container type as parameter
     *        Provides bulk insert into underlying queue.
     * @param value container with elements to insert
     */
    void push(K& value);

    void push(T const&& value);


    void pop(K &target, size_t max);

    size_t pop(char *target, size_t max) {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [this]{ return !queue.empty() || eof; });
        if (queue.empty() && eof) // end of stream
            return 0;

        size_t resultSize = std::min(queue.size(), max);
        std::copy_n(queue.begin(), resultSize, target);
        queue.erase(queue.begin(), queue.begin() + resultSize);
        transferred += resultSize;
        return resultSize;
    }

    T pop() {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [this]{ return !queue.empty() || eof; });
        if (queue.empty() && eof) // end of stream
            return T();

        T rc = queue.back();
        queue.pop_back();
        transferred++;
        return rc;
    }

    bool exhausted() {
        return eof && queue.empty();
    }

    uint64_t getTransferred() const {
        return transferred;
    }

    void markEnd() {
        {
            std::unique_lock<std::mutex> lock(mutex);
            eof = true;
        }
        condition.notify_all();
    }

private:
    std::mutex mutex;
    std::condition_variable condition;
    std::deque<T> queue;

    uint64_t transferred = 0;
    bool eof = false;
};


template<typename T, typename K>
void BlockingQueue<T, K>::push(K &value) {
    {
        std::unique_lock<std::mutex> lock(mutex);
        queue.insert(queue.end(), value.begin(), value.end());
    }
    condition.notify_one();
}

template<typename T, typename K>
void BlockingQueue<T, K>::push(const T &&value) {
    {
        std::unique_lock<std::mutex> lock(mutex);
        queue.emplace_front(std::move(value));
    }
    condition.notify_one();
}

template<typename T, typename K>
void BlockingQueue<T, K>::pop(K &target, size_t max) {
    std::unique_lock<std::mutex> lock(mutex);
    condition.wait(lock, [this]{ return !queue.empty() || eof; });
    if (queue.empty() && eof) // end of stream
        return;

    size_t resultSize = std::min(queue.size(), max);
    target.reserve(target.size() + resultSize);
    std::copy_n(queue.begin(), resultSize, target.begin());
    queue.erase(queue.begin(), queue.begin() + resultSize);
    transferred += resultSize;
}

#endif // BLOCKING_QUEUE_H
