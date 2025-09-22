#pragma once

#include <string>
#include <any>
#include <atomic> // Required for the thread-safe unique ID generator

/**
 * @enum TaskSource
 * @brief A strongly-typed enum to identify the component that created a task.
 *
 * Using a scoped enum (enum class) prevents naming collisions and avoids implicit
 * conversions to integers, leading to safer and more maintainable code.
 */
enum class TaskSource {
    SCHEDULER,  // Task originated from the Scheduler itself (less common).
    EVENT_LOOP, // Task originated from the "JavaScript" execution context.
    API_WORKER  // Task originated from an asynchronous I/O worker (e.g., an API response).
};

/**
 * @enum TaskAction
 * @brief A strongly-typed enum specifying the primary purpose of the task.
 *
 * This typically represents the direction of data flow within the system.
 */
enum class TaskAction {
    REQUEST,    // The task is a request for an operation (e.g., an API call).
    RESPONSE    // The task represents the result of a completed operation.
};

/**
 * @enum TaskType
 * @brief A strongly-typed enum that classifies tasks to model the JavaScript Event Loop behavior.
 *
 * This distinction is crucial for prioritization in the Event Loop: microtasks are
 * executed with higher priority than macrotasks.
 */
enum class TaskType {
    MACROTASK,  // Corresponds to tasks like setTimeout, I/O events.
    MICROTASK   // Corresponds to tasks like promise resolutions (.then(), .catch()).
};

/**
 * @struct Task
 * @brief Represents a self-contained unit of work that is passed between the system's components.
 *
 * This struct acts as a message, carrying all necessary context for its processing.
 * It is designed as a simple data aggregate (Plain Old Data structure).
 */
struct Task {
    long long id;           // A unique identifier for tracking and debugging.
    TaskSource source;      // The component that generated this task.
    TaskAction action;      // The purpose of the task (request or response).
    TaskType type;          // Determines its queueing priority in the Event Loop.
    long long callback_id;  // An ID that maps to the logic to be executed, stored in the ClosureHeap.
    bool is_promise;        // A flag indicating if the task is the result of a promise resolution.
    std::any data;          // A type-safe container for any associated data (the payload).

    /**
     * @brief Generates a new, unique ID in a thread-safe manner.
     * @return A unique long long identifier.
     */
    static long long generate_id() {
        // A static atomic counter ensures that each call, regardless of the calling thread,
        // receives a unique and sequential ID. This avoids the need for a global mutex.
        static std::atomic<long long> counter(0);
        
        // fetch_add is an atomic operation.
        // std::memory_order_relaxed is used as we only need atomicity for the counter itself;
        // we don't need this operation to synchronize other memory accesses between threads.
        // This is the most performant memory order for this use case.
        return counter.fetch_add(1, std::memory_order_relaxed);
    }
};