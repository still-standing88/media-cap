#ifndef MEDIACAP_UTILS_H
#define MEDIACAP_UTILS_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <chrono>

namespace MediaCap {
namespace Utils {

/**
 * @class ThreadSafeQueue
 * @brief A simple thread-safe queue for passing data between threads.
 *
 * This queue supports blocking and timed-blocking pop operations.
 */
template<typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() = default;
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    /**
     * @brief Pushes a new value onto the queue.
     * @param value The value to push.
     */
    void push(T value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(std::move(value));
        m_cond.notify_one();
    }

    /** @brief Pushes without exceeding the supplied item limit. */
    bool try_push(T value, size_t limit) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.size() >= limit) return false;
        m_queue.push(std::move(value));
        m_cond.notify_one();
        return true;
    }

    /**
     * @brief Tries to pop a value from the queue without blocking.
     * @param value Reference to store the popped value.
     * @return True if a value was popped, false otherwise.
     */
    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            return false;
        }
        value = std::move(m_queue.front());
        m_queue.pop();
        return true;
    }

    /**
     * @brief Pops a value from the queue, blocking until a value is available.
     * @return The popped value.
     */
    T wait_and_pop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond.wait(lock, [this] { return !m_queue.empty(); });
        T value = std::move(m_queue.front());
        m_queue.pop();
        return value;
    }

    /**
     * @brief Pops a value from the queue, blocking for a maximum duration.
     * @param value Reference to store the popped value.
     * @param timeout The maximum time to wait.
     * @return True if a value was popped, false if the timeout was reached.
     */
    bool wait_and_pop_for(T& value, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!m_cond.wait_for(lock, timeout, [this] { return !m_queue.empty(); })) {
            return false;
        }
        value = std::move(m_queue.front());
        m_queue.pop();
        return true;
    }

    /**
     * @brief Checks if the queue is empty.
     * @return True if the queue is empty, false otherwise.
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }
    
    /**
     * @brief Gets the current size of the queue.
     * @return The number of elements in the queue.
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

private:
    mutable std::mutex m_mutex;
    std::queue<T> m_queue;
    std::condition_variable m_cond;
};

}
}

#endif
