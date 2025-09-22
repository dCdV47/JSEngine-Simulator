#pragma once

#include <vector>
#include <string>
#include <any> // Required for the instruction's generic payload

/**
 * @enum InstructionType
 * @brief Defines the types of operations that a 'line of code' can represent in the simulation.
 *
 * Each type corresponds to a distinct action that the EventLoop's execution
 * context (`executeStackJS`) can perform.
 */
enum class InstructionType {
    LOG,                // Simulates a `console.log()` call.
    API_REQUEST,        // Simulates an API call like `fetch()`.
    DOM_UPDATE          // Simulates a DOM manipulation (conceptual, not implemented).
    // More types could be added in the future.
};

/**
 * @struct Instruction
 * @brief Represents a single, atomic operation within a Callback.
 *        It is the conceptual equivalent of a line of code in our simulated JavaScript.
 */
struct Instruction {
    InstructionType type;
    
    // Data for the instruction (e.g., the message to log, the API endpoint URL).
    std::string payload;
    
    // A flag to quickly identify instructions that initiate asynchronous API work.
    bool is_api_request;
    
    // If true, this instruction (e.g., an API_REQUEST) should be handled as a promise,
    // affecting task prioritization (its response becomes a microtask).
    bool is_promise;
    
    // The ID of a callback to be executed upon completion (e.g., a .then() block).
    // A value of -1 indicates no callback is directly attached.
    long long then_callback_id = -1;
};

/**
 * @struct Callback
 * @brief Represents a complete 'function' in our simulated JavaScript environment.
 *        It is essentially a sequence of instructions to be executed serially.
 */
struct Callback {
    // Unique identifier for this callback, assigned by the ClosureHeap.
    long long id;

    // A conceptual field to simulate a closure's memory address or unique identity.
    // Not used in the current logic but important for modeling the concept.
    long long associated_closure;
    
    // The sequence of operations that make up the body of this "function".
    std::vector<Instruction> instructions;
};