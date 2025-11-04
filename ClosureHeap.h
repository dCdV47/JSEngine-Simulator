#pragma once

#include "Callback.h" // Includes the Callback/Instruction definitions
#include <map>
#include <mutex>
#include <stdexcept>
#include <vector>
#include <string>
#include <random>   // For modern, high-quality random number generation
#include <limits>   // For std::numeric_limits

/**
 * @class ClosureHeap
 * @brief A thread-safe repository for storing and managing simulated function closures (Callbacks).
 *
 * This class acts as the engine's central memory space for function definitions, simulating
 * the role of the Heap in a real JavaScript runtime. It decouples the ephemeral `Task`
 * objects (which merely reference logic via an ID) from the persistent `Callback` objects
 * (the "execution recipes"). This design is essential for enabling asynchronous operations,
 * as the logic must outlive the initial execution context that created it.
 */
class ClosureHeap {
private:
    // Stores all registered callbacks, mapping a unique ID to a Callback object.
    std::map<long long, Callback> m_callbacks;
    
    // A mutex to ensure that all operations on the m_callbacks map are atomic,
    // preventing race conditions between threads.
    std::mutex m_mutex;

    // A simple counter to generate unique, sequential IDs for new callbacks.
    long long m_next_id = 0;

    // A high-quality random number engine, seeded once for performance.
    std::mt19937 m_random_engine;

    // A distribution to map the engine's output to the full range of positive long longs.
    std::uniform_int_distribution<long long> m_distribution;

public:
    /**
     * @brief Constructs the ClosureHeap and initializes the random number generator.
     */
    ClosureHeap()
        : m_random_engine(std::random_device{}()), // Seed with a hardware-based non-deterministic value
          m_distribution(1, std::numeric_limits<long long>::max())
    {}

    // The ClosureHeap manages shared, mutable state and is a singleton-like resource,
    // so copying or assigning it would be a design error.
    ClosureHeap(const ClosureHeap&) = delete;
    ClosureHeap& operator=(const ClosureHeap&) = delete;

    /**
     * @brief Registers a sequence of instructions as a new Callback.
     * @param instructions The vector of instructions that defines the callback's logic. They represent JS Code.
     * @return The unique ID assigned to the newly registered Callback.
     */
    long long register_callback(std::vector<Instruction> instructions) {
        std::lock_guard<std::mutex> lock(m_mutex);
        long long id = m_next_id++;
        
        // Generate a random ID to simulate the unique memory address of a new closure environment.
        long long closure_id = m_distribution(m_random_engine);
        
        m_callbacks[id] = {id, closure_id, std::move(instructions)};
        return id;
    }

    /**
     * @brief Retrieves a copy of the Callback associated with a given ID.
     * @param id The unique ID of the callback to retrieve.
     * @return A copy of the Callback object. Returning by value is a deliberate choice
     *         to ensure the caller has a safe, isolated snapshot of the instructions,
     *         preventing race conditions if the original were to be modified.
     * @throws std::runtime_error if no callback with the specified ID is found.
     */
    Callback get(long long id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        try {
            // .at() provides bounds checking and throws std::out_of_range if the key doesn't exist.
            return m_callbacks.at(id);
        } catch (const std::out_of_range& e) {
            // Wrap the specific exception in a more general one with a descriptive message for easier debugging.
            throw std::runtime_error("Callback ID not found: " + std::to_string(id));
        }
    }
};