#include <iostream>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include <any>
#include <atomic>
#include <utility>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <random>
#include <limits>

#include "Task.h"
#include "ClosureHeap.h"
#include "Alarm.h"
#include "TaskQueue.h"

// ===================================================================
// == INTERACTIVE SIMULATION FUNCTIONS
// ===================================================================

/**
 * @brief Simulates a chained promise scenario, like `fetch(...).then(...)`.
 * This function sets up the entire chain of callbacks and injects the initial
 * task into the engine to kick off the process.
 */
void simulateFetchThen(ClosureHeap& cb_manager, TaskQueue<Task>& sched_q, Alarm& sched_alarm) {
    std::cout << "\n[MAIN]: === SIMULATION: Chained Promise (fetch.then) ===" << std::endl;

    // STEP 1: Define the terminal callback (`.then()` clause of the second promise).
    // This represents the final action to be taken after all async work is complete.
    long long final_cb_id = cb_manager.register_callback({
        {InstructionType::LOG, "SUCCESS: The chained promise was resolved and its final callback executed.", false, false, -1}
    });

    // STEP 2: Define the initial callback, which chains the second promise.
    // This simulates the code inside the first `.then()`, which triggers another async operation.
    long long initial_cb_id = cb_manager.register_callback({
        {InstructionType::LOG, "First promise resolved. Dispatching a new API request from its callback...", false, false, -1},
        // This instruction simulates: fetch("api/details").then(finalHandler)
        {InstructionType::API_REQUEST, "api/user/details", true, true, final_cb_id}
    });

    // STEP 3: Inject the initial task, simulating the resolution of the first promise.
    std::cout << "[MAIN]: Injecting initial API response to trigger the promise chain..." << std::endl;
    Task first_promise_task;
    first_promise_task.id = Task::generate_id();
    first_promise_task.source = TaskSource::API_WORKER;
    first_promise_task.action = TaskAction::RESPONSE;
    first_promise_task.callback_id = initial_cb_id;
    first_promise_task.is_promise = true;
    first_promise_task.data = std::string("Initial API response data");

    sched_q.push_back(std::move(first_promise_task));
    sched_alarm.notify();
    std::cout << "[MAIN]: =================================================\n" << std::endl;
}

/**
 * @brief Simulates a user-initiated DOM event, like a button click.
 * This function demonstrates the macrotask pathway. The task is not a promise and
 * will be executed by the Event Loop only after any pending microtasks are cleared.
 */
void simulateDomClick(ClosureHeap& cb_manager, TaskQueue<Task>& sched_q, Alarm& sched_alarm) {
    std::cout << "\n[MAIN]: === SIMULATION: DOM Click Event (Macrotask) ===" << std::endl;

    // STEP 1: Define the 'onclick' event handler.
    long long on_click_cb_id = cb_manager.register_callback({
        {InstructionType::LOG, "SUCCESS: DOM event processed! The button's 'onclick' handler was executed.", false, false, -1}
    });

    // STEP 2: Create the task that simulates the click event.
    // Note that `is_promise` is false, marking this as a standard macrotask.
    std::cout << "[MAIN]: Injecting DOM event task into the engine..." << std::endl;
    Task dom_event_task;
    dom_event_task.id = Task::generate_id();
    dom_event_task.source = TaskSource::API_WORKER; // The "DOM API" is another external source
    dom_event_task.action = TaskAction::RESPONSE;
    dom_event_task.type = TaskType::MACROTASK;
    dom_event_task.callback_id = on_click_cb_id;
    dom_event_task.is_promise = false;
    dom_event_task.data = std::string("{\"type\":\"click\", \"target\":\"#submit-btn\"}");

    // STEP 3: Inject the task and notify the Scheduler.
    sched_q.push_back(std::move(dom_event_task));
    sched_alarm.notify();
    std::cout << "[MAIN]: =============================================\n" << std::endl;
}

// Represents a request destined for an external API worker.
struct ApiRequest {
    long long task_id;
    TaskAction action = TaskAction::REQUEST;
    std::any data;
};

// Represents a response coming from an external API worker.
struct ApiResponse {
    long long task_id;
    TaskAction action = TaskAction::RESPONSE;
    std::any data;
};


/**
 * @brief Simulates sending a request to an external API.
 *
 * This function launches a new "fire-and-forget" thread to simulate a non-blocking
 * network operation. Upon completion, the worker thread places the response in the
 * ApiManager's response queue and notifies it to wake up and process the result.
 *
 * @param request The request object containing the data for the API call.
 * @param response_queue A reference to the queue where the worker must place the response.
 * @param api_manager_alarm A reference to the ApiManager's alarm to notify it upon completion.
 */
void sendAPIRequest(
    ApiRequest request,
    TaskQueue<ApiResponse>& response_queue,
    Alarm& api_manager_alarm
) {
    // Launch the work in a new thread to avoid blocking the ApiManager.
    std::thread worker_thread(
        // The lambda that contains the code to be executed in the new thread.
        [](ApiRequest req, TaskQueue<ApiResponse>& resp_q, Alarm& alarm) {
            
            std::cout << "    [API Worker " << req.task_id << "]: Request received. Starting simulated work..." << std::endl;

            // Simulate network latency.
            std::this_thread::sleep_for(std::chrono::seconds(2));

            // Prepare the response.
            ApiResponse response;
            response.task_id = req.task_id;
            response.data = std::string("{\"message\":\"API data received successfully\"}");

            std::cout << "    [API Worker " << req.task_id << "]: Work complete. Enqueuing response..." << std::endl;

            // Push the ApiResponse into the ApiManager's queue.
            resp_q.push_back(response);

            // Notify the ApiManager in case it's sleeping.
            alarm.notify();

            // The thread terminates here, and its resources are released.
        },
        std::move(request),             // Move the request to transfer ownership.
        std::ref(response_queue),       // Pass the queue by reference.
        std::ref(api_manager_alarm)     // Pass the alarm by reference.
    );

    // .detach() allows the thread to run independently.
    // The ApiManager does not need to wait (join()) for it to finish.
    worker_thread.detach();
}

/**
 * @brief Simulates the execution of code on the JavaScript Call Stack.
 *
 * This function runs on the EventLoop thread. It interprets a sequence of
 * instructions from a Callback and performs actions based on them, such as
 * logging messages or creating new tasks for the Scheduler.
 *
 * @param callback The Callback object containing the instructions to execute.
 * @param data The input data for this execution (e.g., the response from an API).
 * @param closure_heap Reference to the Closure Heap to register new functions.
 * @param scheduler_queue Reference to the Scheduler's queue to send new tasks.
 * @param scheduler_alarm Reference to the Scheduler's alarm to wake it up.
 */
void executeStackJS(
    Callback callback,
    const std::any& data,
    ClosureHeap& closure_heap,
    TaskQueue<Task>& scheduler_queue,
    Alarm& scheduler_alarm
) {
    std::cout << "  [EventLoop::executeStackJS] >>>> STARTING EXECUTION OF CALLBACK ID: " << callback.id << std::endl;

    // Print the data received by the task, if any.
    try {
        if (data.has_value()) {
            // For this simulation, we assume the data is a printable string.
            std::cout << "  [EventLoop::executeStackJS] Data received: \"" << std::any_cast<const std::string&>(data) << "\"" << std::endl;
        }
    } catch (const std::bad_any_cast& e) {
        std::cout << "  [EventLoop::executeStackJS] Received data of a non-printable type." << std::endl;
    }

    // Iterate and "interpret" each instruction within the callback.
    for (const auto& instruction : callback.instructions) {
        std::cout << "  [EventLoop::executeStackJS] Executing instruction: " << instruction.payload << std::endl;

        // If the instruction is an API request, we need to generate a new Task.
        if (instruction.is_api_request) {
            std::cout << "  [EventLoop::executeStackJS] Instruction is an API Request! Creating new task..." << std::endl;

            // Register the code to exucute after then() for this API Request
            long long response_callback_id = instruction.then_callback_id;

            if (response_callback_id == -1) {
                std::cout << "  [EventLoop::executeStackJS] ADVERTENCIA: API Request sin .then() callback. La respuesta se perderá." << std::endl;
            }

            // 3. Create a new Task to be sent to the Scheduler.
            Task api_request_task;
            api_request_task.id = Task::generate_id();
            api_request_task.source = TaskSource::EVENT_LOOP;
            api_request_task.action = TaskAction::REQUEST;
            api_request_task.type = instruction.is_promise ? TaskType::MICROTASK : TaskType::MACROTASK;
            api_request_task.callback_id = response_callback_id; // <- The ID of the response callback.
            api_request_task.is_promise = instruction.is_promise;
            api_request_task.data = instruction.payload; // e.g., The URL/endpoint for the API.

            std::cout << "  [EventLoop::executeStackJS] Task (ID " << api_request_task.id << ") created. Dispatching to Scheduler." << std::endl;

            // 4. Enqueue the task in the Scheduler's queue and notify it.
            scheduler_queue.push_back(api_request_task);
            scheduler_alarm.notify();
        }
    }

    std::cout << "  [EventLoop::executeStackJS] <<<< FINISHED EXECUTION OF CALLBACK ID: " << callback.id << std::endl;
}

int main() {
    std::cout << "[Scheduler/Main]: Initializing engine..." << std::endl;

    // The ClosureHeap serves as the engine's central memory space, simulating the Heap
    // in a real JavaScript runtime. Its role is to store all function definitions (Callback objects)
    // so they persist beyond the execution scope that creates them. This provides the critical
    // decoupling between a `Task` (a transient message carrying a callback_id) and the `Callback`
    // (the persistent logic to be executed). Essentially, it's the source of truth for all
    // executable logic in the engine.
    ClosureHeap closure_heap;

    // 1. Create the necessary communication queues for inter-thread messaging.
    TaskQueue<Task> scheduler_queue;
    TaskQueue<Task> api_manager_request_queue;
    TaskQueue<ApiResponse> api_manager_response_queue;
    TaskQueue<Task> event_loop_macrotask_queue;
    TaskQueue<Task> event_loop_microtask_queue;
    std::cout << "[Scheduler/Main]: Task queues created." << std::endl;

    // Create 3 alarms, one for each main actor thread.
    // Each alarm's wake-up condition is a lambda that checks if its actor's queue(s) are non-empty.
    Alarm scheduler_alarm([&]() { return !scheduler_queue.isEmpty(); });
    Alarm api_manager_alarm([&]() { return !api_manager_request_queue.isEmpty() || !api_manager_response_queue.isEmpty(); });
    Alarm event_loop_alarm([&]() {
        return !event_loop_macrotask_queue.isEmpty() || !event_loop_microtask_queue.isEmpty();
    });
    std::cout << "[Scheduler/Main]: Alarms created and configured." << std::endl;

    // 2. Launch the Scheduler Thread.
    // This thread acts as a central router, directing tasks from their source to their destination.
    std::thread scheduler_thread([&]() {
        std::cout << "[Scheduler]: Thread started." << std::endl;

        while (true) { // The Scheduler's main loop.

            // 1. Process all pending tasks in its queue.
            while (!scheduler_queue.isEmpty()) {
                Task task = scheduler_queue.pop();
                std::cout << "[Scheduler]: Popped Task (ID " << task.id << "). Analyzing source..." << std::endl;

                // 2. Route the task based on its origin.
                if (task.source == TaskSource::API_WORKER) {
                    // Task comes from an API response, destined for the EventLoop.
                    if (task.is_promise) {
                        std::cout << "  [Scheduler] API task is a promise. Routing to MICROTASK queue." << std::endl;
                        event_loop_microtask_queue.push_back(std::move(task));
                    } else {
                        std::cout << "  [Scheduler] API task is standard. Routing to MACROTASK queue." << std::endl;
                        event_loop_macrotask_queue.push_back(std::move(task));
                    }
                    
                    // Wake up the EventLoop to process the new task.
                    event_loop_alarm.notify();

                } else if (task.source == TaskSource::EVENT_LOOP) {
                    // Task comes from the Call Stack (JS), it's a request for the ApiManager.
                    std::cout << "  [Scheduler] EventLoop task. Routing to API_MANAGER queue." << std::endl;
                    
                    api_manager_request_queue.push_back(std::move(task));
                    
                    // Wake up the ApiManager to process the new request.
                    api_manager_alarm.notify();
                
                } else {
                    // Handle other cases or potential errors.
                    std::cerr << "  [Scheduler] WARNING: Task with unhandled source detected." << std::endl;
                }
            }

            // If the queue is empty, go to sleep until notified.
            std::cout << "[Scheduler]: Queue empty. Going to sleep..." << std::endl;
            scheduler_alarm.wait();
            std::cout << "[Scheduler]: Woken up by a notification." << std::endl;
        }
    });
    std::cout << "[Main]: Scheduler thread launched." << std::endl;


    // 3. Launch the API Manager Thread.
    // This thread manages asynchronous I/O operations, spawning workers and processing their results.
    std::thread api_manager_thread([&]() {
        std::cout << "[ApiManager]: Thread started." << std::endl;

        // Hash map to maintain the context of in-flight API requests.
        // Key: task_id, Value: The original Task object.
        std::unordered_map<long long, Task> pending_api_tasks;

        while (true) { // The ApiManager's main loop.  

            // --- PHASE 1: PROCESS NEW REQUESTS ---
            while (!api_manager_request_queue.isEmpty()) {
                // Dequeue the task and store it in our map of pending operations.
                Task task = api_manager_request_queue.pop();
                std::cout << "[ApiManager]: New request RECEIVED (ID: " << task.id << "). Storing context..." << std::endl;
                pending_api_tasks[task.id] = task;

                // Prepare the request object for the worker thread.
                // This is for security: we don't want to share internal/unnecesary data with external APIs
                ApiRequest request_to_api;
                request_to_api.task_id = task.id;
                request_to_api.data = task.data;

                std::cout << "  [ApiManager] Launching worker for Task ID: " << task.id << std::endl;
                sendAPIRequest(
                    std::move(request_to_api),
                    api_manager_response_queue, // The queue for workers to send responses to.
                    api_manager_alarm           // This manager's own alarm for notification.
                );
            }

            // --- PHASE 2: PROCESS COMPLETED RESPONSES ---
            while (!api_manager_response_queue.isEmpty()) {
                ApiResponse api_response = api_manager_response_queue.pop();
                std::cout << "[ApiManager]: Response RECEIVED for Task ID: " << api_response.task_id << ". Looking up context..." << std::endl;

                // Find the original task in the hash map to retrieve its context (e.g., callback_id).
                auto pending_task_it = pending_api_tasks.find(api_response.task_id);
                if (pending_task_it != pending_api_tasks.end()) {
                    std::cout << "  [ApiManager] Context FOUND for Task ID: " << api_response.task_id << ". Re-composing and dispatching to Scheduler." << std::endl;
                    Task completed_task = std::move(pending_task_it->second); 
                    pending_api_tasks.erase(pending_task_it);

                    // Re-hydrate the task with the response data and update its source.
                    completed_task.source = TaskSource::API_WORKER;  
                    completed_task.data = api_response.data;  
                    
                    // Promises (microtasks) often have higher priority. While this simulation doesn't use a priority queue,
                    // pushing to the front achieves a similar effect for immediate processing.
                    if(completed_task.is_promise){
                        std::cout << "    [ApiManager] Task ID " << completed_task.id << " is a promise. Sending with high priority (front)." << std::endl;
                        scheduler_queue.push_front(completed_task);
                    }else{
                        std::cout << "    [ApiManager] Task ID " << completed_task.id << " is standard. Sending with normal priority (back)." << std::endl;
                        scheduler_queue.push_back(completed_task);
                    }
                    
                    std::cout << "  [ApiManager] Notifying Scheduler." << std::endl;
                    scheduler_alarm.notify(); // Wake up the scheduler.
                } else {
                     // This is a critical error to log, as it indicates a state mismatch.
                    std::cerr << "  [ApiManager] ERROR! No context found for Task ID: " << api_response.task_id << ". Discarding response." << std::endl;
                }
            }

            // --- PHASE 3: WAIT ---
            // If there's no activity in either queue, go to sleep.
            std::cout << "[ApiManager]: No pending activity. Going to sleep..." << std::endl;
            api_manager_alarm.wait();
            std::cout << "[ApiManager]: Woken up by a notification." << std::endl;
        }
    });
    std::cout << "[Main]: ApiManager thread launched." << std::endl;


    // 4. Launch the Event Loop Thread.
    // This thread simulates the single-threaded nature of JavaScript's execution environment.
    std::thread event_loop_thread([&]() {
        std::cout << "[EventLoop]: Thread started." << std::endl;
        while (true) { // The EventLoop's main loop.

            // Phase 1: Process ONE macrotask (if available).
            // This models how browsers handle one macrotask per event loop tick.
            if (!event_loop_macrotask_queue.isEmpty()) {
                Task macro_task = event_loop_macrotask_queue.pop();

                // Retrieve the associated callback and execute it.
                // TO-DO: add error handling
                Callback cb_to_run = closure_heap.get(macro_task.callback_id);
                executeStackJS(cb_to_run, macro_task.data, closure_heap, scheduler_queue, scheduler_alarm);
            }
            
            // Phase 2: Process ALL pending microtasks.
            // Microtasks (like promise resolutions) are executed exhaustively after each macrotask.
            while (!event_loop_microtask_queue.isEmpty()) {
                Task micro_task = event_loop_microtask_queue.pop();

                // Retrieve the associated callback and execute it.
                // TO-DO: add error handling
                Callback cb_to_run = closure_heap.get(micro_task.callback_id);
                executeStackJS(cb_to_run, micro_task.data, closure_heap, scheduler_queue, scheduler_alarm);
            }

            // Phase 3: If both queues are empty, wait for a new task.
            if (event_loop_macrotask_queue.isEmpty() && event_loop_microtask_queue.isEmpty()) {
                std::cout << "[EventLoop]: No more tasks. Going to sleep..." << std::endl;
                event_loop_alarm.wait();
                std::cout << "[EventLoop]: Woken up by a notification." << std::endl;
            }
        }
    });

    std::cout << "[Main]: All actor threads have been launched." << std::endl;
    std::cout << "--------------------------------------------------------\n" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1)); // Allow time for threads to initialize and go to sleep.

    // --- 3. INTERACTIVE COMMAND LOOP ---
    while (true) {
        std::cout << "\n==================== JS ENGINE CONTROL PANEL ====================" << std::endl;
        std::cout << "Choose an action to inject into the engine:" << std::endl;
        std::cout << "  1. Simulate a chained promise (fetch().then())" << std::endl;
        std::cout << "  2. Simulate a DOM click event (macrotask)" << std::endl;
        std::cout << "  q. Quit" << std::endl;
        std::cout << "=================================================================" << std::endl;
        std::cout << "> ";

        char choice;
        std::cin >> choice;

        switch (choice) {
            case '1':
                simulateFetchThen(closure_heap, scheduler_queue, scheduler_alarm);
                std::this_thread::sleep_for(std::chrono::seconds(4)); // Pause to allow user to read output
                break;
            case '2':
                simulateDomClick(closure_heap, scheduler_queue, scheduler_alarm);
                std::this_thread::sleep_for(std::chrono::seconds(1));
                break;
            case 'q':
            case 'Q':
                std::cout << "[MAIN]: Shutdown initiated." << std::endl;
                // In a real app, you would signal threads to exit gracefully.
                // For this simulation, we just exit.
                return 0;
            default:
                std::cout << "[MAIN]: Invalid option. Please try again." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
                break;
        }
        
        // Clear the input buffer
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    // These will only be reached if the loop is broken without exiting.
    scheduler_thread.join();
    api_manager_thread.join();
    event_loop_thread.join();

    return 0;
}