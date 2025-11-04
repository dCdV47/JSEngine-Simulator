#pragma once

#include <deque>
#include <mutex>
#include <utility> // For std::move

/**
 * @class TaskQueue
 * @brief A generic, thread-safe queue for inter-thread communication.
 *
 * This class provides a synchronization wrapper around a std::deque to ensure
 * that operations like push and pop can be safely called from multiple threads
 * without causing data races. It is templated to allow storing any type of object.
 *
 * @tparam T The type of elements to be stored in the queue.
 */
template <typename T>
class TaskQueue {
private:
    std::deque<T> m_tasks;
    std::mutex m_mutex;

public:
    // Default constructor is sufficient.
    TaskQueue() = default;

    // A thread-safe queue manages shared state (the mutex and the deque).
    // Copying it would be ambiguous and error-prone. Deleting these operations
    // prevents such complexities and enforces a single point of ownership.
    TaskQueue(const TaskQueue&) = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;

    /**
     * @brief Pushes an item to the front of the queue.
     * This is useful for high-priority items (like microtasks) that need
     * to be processed before others.
     * @param item The item to be added. The item is moved into the queue for efficiency.
     */
    void push_front(T item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tasks.push_front(std::move(item));
    }

    /**
     * @brief Pushes an item to the back of the queue (standard FIFO behavior).
     * @param item The item to be added. The item is moved into the queue for efficiency.
     */
    void push_back(T item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tasks.push_back(std::move(item));
    }

    /**
     * @brief Pops and returns the item from the front of the queue.
     * @return The front-most item. If the queue is empty, a default-constructed
     *         object of type T is returned (e.g., 0 for int, "" for std::string).
     */
    T pop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_tasks.empty()) {
            return T{}; // Return a default-constructed value for type T
        }
        T item = std::move(m_tasks.front());
        m_tasks.pop_front();
        return item;
    }

    /**
     * @brief Thread-safely checks if the queue is empty.
     * @return true if the queue contains no elements, false otherwise.
     */
    bool isEmpty() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tasks.empty();
    }
};