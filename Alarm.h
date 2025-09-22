#pragma once

#include <mutex>
#include <condition_variable>
#include <functional>

/**
 * @class Alarm
 * @brief A decoupled, condition-based synchronization primitive for threads.
 *
 * This class allows a thread to wait efficiently (without busy-waiting)
 * until a specific condition is met. Another thread can "notify" the Alarm,
 * prompting the waiting thread to re-evaluate its condition and wake up if it is satisfied.
 *
 * The wake-up condition is provided as a predicate at construction, making this
 * a highly flexible and reusable component for managing inter-thread communication.
 * It encapsulates the standard C++ pattern of using a std::condition_variable with
 * a mutex and a predicate, simplifying its usage and reducing boilerplate.
 */
class Alarm {
private:
    std::mutex m_mutex;
    std::condition_variable m_cond_var;
    std::function<bool()> m_wakeup_condition;

public:
    /**
     * @brief Constructs an Alarm object.
     * @param condition A predicate function (typically a lambda) that takes no arguments
     *        and returns 'true' when the waiting thread should wake up, or 'false' if it should
     *        continue waiting. This function will be invoked safely under the protection of the internal mutex.
     */
    explicit Alarm(std::function<bool()> condition)
        : m_wakeup_condition(std::move(condition))
    {
    }

    // Alarms manage unique synchronization state and are therefore non-copyable and non-movable.
    // Deleting these prevents potential race conditions and incorrect state management.
    Alarm(const Alarm&) = delete;
    Alarm& operator=(const Alarm&) = delete;

    /**
     * @brief Puts the calling thread into a waiting state.
     *
     * The thread will block efficiently until another thread calls notify()
     * AND the wake-up condition (provided in the constructor) returns 'true'.
     * This implementation is robust against spurious wakeups because it uses the
     * predicate-based version of std::condition_variable::wait.
     */
    void wait() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond_var.wait(lock, m_wakeup_condition);
    }

    /**
     * @brief Notifies a waiting thread to re-evaluate its condition.
     *
     * This wakes up ONE thread that is currently blocked in a call to wait().
     * If no threads are waiting, this call has no effect. This is the signal
     * that the state protected by the alarm's predicate may have changed.
     */
    void notify() {
        m_cond_var.notify_one();
    }
};