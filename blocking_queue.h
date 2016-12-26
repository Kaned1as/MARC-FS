#ifndef BLOCKING_QUEUE_H
#define BLOCKING_QUEUE_H


#include <mutex>
#include <condition_variable>
#include <deque>

template <typename T>
class BlockingQueue
{
public:
    void push(T const& value) {
        {
            std::unique_lock<std::mutex> lock(this->mutex);
            queue.push_front(value);
        }
        this->condition.notify_one();
    }

    void push(T const&& value) {
        {
            std::unique_lock<std::mutex> lock(this->mutex);
            queue.emplace_front(value);
        }
        this->condition.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(this->mutex);
        this->condition.wait(lock, [this]{ return !queue.empty() || eof; });
        if (queue.empty() && this->eof) // end of stream
            return T();

        T rc = this->queue.back();
        this->queue.pop_back();
        return rc;
    }

    bool exhausted() {
        return this->eof && this->queue.empty();
    }

    void markEnd() {
        {
            std::unique_lock<std::mutex> lock(this->mutex);
            this->eof = true;
        }
        this->condition.notify_all();
    }

private:
    std::mutex              mutex;
    std::condition_variable condition;
    std::deque<T>           queue;
    bool                    eof;
};

#endif // BLOCKING_QUEUE_H
