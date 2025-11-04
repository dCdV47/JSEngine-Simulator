# JS ENGINE MODEL

## 0- Everything is managed by the browser's engine / Node.js server (V8, SpiderMonkey)

## 1- API Manager - handles API requests and responses

This engine sees the "outside world" through Web/C++ APIs.
There are different APIs with many distinct connected functions, and they are all responsible for notifying the engine that something has happened in the outside world.
There are several, to understand them:

*   **"DOM" API** - is responsible for detecting changes in the DOM (clicks, key presses...) and making changes to it.
*   **"WebSocket" API** - is responsible for opening, closing, and managing incoming and outgoing socket messages (on.message, send()...).
*   **"IndexedDB" API** - is responsible for communicating with the browser's internal database, either to retrieve or insert data.
*   **"Tab Manager" API** - is responsible for communicating with other open browser tabs.
*   ...
*   **SPECIAL:**
    *   **"Render" API** - Paints the screen in Chrome, Firefox, etc. It's not an API as such, but a separate program that runs in the browser at 60fps and synchronizes with the data from the EVENT LOOP.

### *REGARDING API CALLS, MACROTASKS/MICROTASKS, and Promises*

a) Each time we make a request to an API from JS code, when this API returns the data for a Callback to be executed, this Callback will be enqueued as a **MACROTASK**.

b) It is us, by wrapping these calls in Promises, who manage to get them enqueued as a **MICROTASK** when they resolve.

c) There are 2 exceptions for APIs whose responses go directly to the **MICROTASKS** queue:
*   `fetch()`: wraps the HTTP request in a Promise so that it is enqueued as a **MICROTASK** when the response is received.
*   `MutationObserver`: observes changes in the DOM. It groups the changes and executes its callback as a **microtask** to ensure you react to DOM changes before the browser repaints the screen.

## 2- In the Setup phase, the engine does everything and creates different elements.

a) **CALL STACK**: is where I execute JS code. It's a single thread. I push code onto it, execute it, and when it finishes executing, I empty it. One execution at a time.

The code is executed line by line, until:
1.  The main function of the task finishes (due to a `return` or end of code).

2.  The main function of the task is `ASYNC` and encounters an `await`.
    At that moment, a `Promise<pending/resolved>` is created, and the code of the function is saved in its `functionCallback` until:
    *   the next `await`
    *   the function's `return` or the end of the code.

    **NOTE:** this promise is almost always created as "pending". It could be created as "resolved" in a case like:

```javascript
function exampleWrapper() {
    // A message arrives via the WebSocket (MACROTASK)
    webSocket.onmessage = async (data) => {

        const savedMessages = await saveMessagesToDB(data?.payload?.messages);
        // a <fulfilled> Promise is created instantly because there are no messages

        /*
        the rest of the code will be enqueued as a MICROTASK instantly (since the Promise is already resolved)
        ...
        */
        return savedMessages;
    }
}

function saveMessagesToDB(messages, storeName = MSG_STORE_NAME) {
    // we enter here, the promise resolves directly
    if (!db || !messages || messages.length === 0) return Promise.resolve();

    return new Promise((resolve, reject) => {
        const transaction = db.transaction([storeName], 'readwrite');
        const store = transaction.objectStore(storeName);

        messages.forEach(msg => {
            store.put(msg); // 'put' is an "upsert" operation.
        });

        transaction.oncomplete = () => resolve(messages);
        transaction.onerror = (event) => reject(event.target.error);
    });
}
/* here the promise is resolved directly, and the callback is enqueued as a MICROTASK */
```

3.  In any other case, the code will continue.
    For example: An asynchronous API like `setTimeout`, `fetch`, etc., is invoked.
    The API call itself is synchronous; the callback is scheduled for later, and the execution of the current code continues.

b) **MACROTASK QUEUE**: is where I put the "tasks" triggered directly by Web/C++ API events (onClick, database response, etc).

c) **MICROTASK QUEUE**: is where I put the "callbacks" from Resolved Promises.

d) **RENDERER**: is where I store and modify the information to render the DOM.
* *I am the guardian of the screen.*
* *I synchronize with the DOM API to tell it to update and with the Render API to tell it what to display.*

e) **MEMORY/HEAP**: Is where I store the variables/objects created by the JS in the CALL STACK.
* *The guardian of closures. I synchronize very well with the CALL STACK to make "JS closures" work correctly: I pass the correct value of each variable to the JS engine at every moment, even if they have the same name in the JS code. I am the one who enables the magic of closures, greatly supporting the fantasy of "JS multithreading".*
* *The guardian of Promises. When JS creates a promise, I am responsible for saving it with a "pending" state in memory and placing the code that needs to be executed when this promise resolves into its `functionCallback`.*

f) **SCHEDULER**: I am responsible for organizing the execution order of the EVENT LOOP.
* *When a Web/C++ API resolves a task (like the response to a fetch request) or notifies me of an event (e.g., the user clicked a button), I am responsible for assigning that task to the MICROTASK or MACROTASK QUEUE.*
    1.  If it's a normal task (API event, click, socket, etc): I enqueue it in the **MACROTASK QUEUE**.
    2.  If it's a task originating from the CALL STACK, wrapped as a `Promise` or `await` (usually fetch, DB, etc):
        a) I notify MEMORY that the Promise has resolved so it can change the state to "fulfilled" or "rejected".
        b) I retrieve its `functionCallback` and enqueue it in the **MICROTASK QUEUE**.

h) **EVENT LOOP**: I am responsible for managing the execution of the CALL STACK and communicating with the rendering API.

z) ... I'm sleeping, and something wakes me up:
*   an API sends me data because a previous fetch arrived
*   because a user clicked on a button
*   a JS script was loaded from HTML...

*We could summarize it as: I had nothing pending, but now the SCHEDULER has assigned me a new MACROTASK or MICROTASK*

a) **if:** Are there pending tasks in the **MACROTASK QUEUE**? I DO 1, only 1.
    => I move the first MACROTASK (the first one that entered, FIFO) from the QUEUE to the CALL STACK and execute it.

b) **while:** As long as there are pending tasks in the **MICROTASK QUEUE**, I execute them one after another until it's empty.
    => I move the first MICROTASK (the first one that entered, FIFO) from the QUEUE to the CALL STACK and execute it.

c) I call the **RENDERER** (if necessary).

d) I check, and if there's nothing pending, I go to sleep...

### THIS IS THE KEY INNOVATION

*   This prioritizes MICROTASKS (crucial for managing concurrency).
*   This allows for the chaining of `.then()` or `await` Promises (crucial for chaining priority Web/C++ API requests).

#### Practical Example: Node.js Server

1.  111 different users send us a request through the WebSockets API at approximately the same time.

2.  The **SCHEDULER** enqueues all these calls in the **MACROTASK QUEUE** in order of arrival.

3.  **USER 1**
    The **EVENT LOOP** executes the first macrotask on the **CALL STACK**. This execution includes a promise that makes a request to the DB API.
    a) This promise remains in **MEMORY** as "pending" and is assigned its "Callback function".
    b) The DB API starts working on its own to retrieve the requested data.

4.  The **CALL STACK** is emptied after executing everything.

5.  The **EVENT LOOP** sees that the **MICROTASK QUEUE** is empty, so it renders, and the loop starts again.

6.  ... this happens identically for the first 10 users.

7.  **USER 11**: here things change. While executing the **MACROTASK** for USER 11 on the **CALL STACK**...
    **THINGS ARE HAPPENING IN THE ENGINE!!!**
    *   The DB APIs have returned 3 results. The data awaited by users 1, 3, and 7 has been fulfilled.
    *   The **SCHEDULER** takes these Promises that wrapped the DB API calls, marks them as `fulfilled`, gets their "Callback functions", and adds them to the **MICROTASK QUEUE**.

8.  While the **SCHEDULER** was doing its job (in parallel, it's completely independent of the CALL STACK execution)...
    *   The execution of the **MACROTASK** for USER 11 on the **CALL STACK** has finished, and the **CALL STACK** is now empty.
    *   Then, the **EVENT LOOP** looks and sees that the **MICROTASK QUEUE** now has tasks!
        *   It pushes them one by one onto the **CALL STACK** until there are none left.
        *   ***IMPORTANT NOTE:*** *The MICROTASK for USER 1 generates another Promise during its execution.*

9.  While the **MICROTASK** for USER 7 was being resolved, the DB API returned the data for USER 11 (he just arrived and already has it, what a lucky guy!) and also the data from the other promise. The **SCHEDULER**, without a second thought, marks the promise(s) as fulfilled and adds their "Callback functions" to the **MICROTASK QUEUE**.

10. The **EVENT LOOP** sees that the **MICROTASK QUEUE** is empty, all tasks have been executed:
    a) The initial promises for users 1, 3, 7, and 11 (the lucky one!).
    b) The chained promise from the MICROTASK of USER 1.

11. Now, and only now, does the **EVENT LOOP** move on to the **MACROTASK queue** to handle the next request, the one from USER 12.

> **Reflection:** What does this achieve? Well, users 1, 3, 7, and 11 are already served!
> If the MICROTASK QUEUE didn't exist, all these responses from the DB API would have been assigned to the MACROTASK QUEUE (as happened in the past, when the MICROTASK QUEUE did not exist), and the requests of 100 more users would still need to be handled before completely resolving the requests of the earlier users.
>
> This would be a terrible user experience and a huge concurrency problem. Node.js would make no sense.
>
> However, with this "little trick," the Node.js engine functions like a multi-threaded server even though there is only one JS thread running on the CALL STACK.

**P.S.:** In the browser, it would be the same. Before handling new DOM events, the engine checks if there are previous requests (promises) that were left pending before tackling new tasks. This allows everything to be more responsive.