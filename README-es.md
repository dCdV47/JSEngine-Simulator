# Simulador de Motor JavaScript en C++

El objetivo de esta simulación del motor JS es demostrar de manera práctica y visual los conceptos fundamentales de la concurrencia asíncrona, como el **Event Loop**, las colas de **Macro Tareas** y **Micro Tareas**, la gestión de operaciones de I/O (API's) y el **Closure Heap**.

> 📘 **This document is also available in English:**  
> [Read in English](README.md)

La simulación está construida sobre una arquitectura multi-hilo que aísla los componentes principales del motor, permitiendo observar cómo interactúan para procesar tareas sin bloquear el hilo de ejecución principal, imitando el comportamiento de entornos como Node.js o el navegador.

## Arquitectura y Flujo de Ejecución

El siguiente diagrama es la pieza central para entender el flujo de ejecución del sistema.

![Diagrama de Arquitectura del Motor JS](JSEngineDiagram.jpg)

Si quieres entender el profundizar en mis razonamientos del modelo JS "asíncrono": ➡️ **[Lee el JSEngine Blueprint completo aquí](./JSEngine-Blueprint-es.md)**

### Componentes Principales

1.  **Event Loop**: Es el corazón del motor. Orquesta la ejecución de tareas siguiendo un ciclo estricto:
    *   1. Ejecuta **UNA** Macro Tarea de su cola.
    *   2. Ejecuta **TODAS** las Micro Tareas hasta que la cola esté vacía.
    *   3. (Opcional) Procede a renderizar cambios.
    *   4. Si no hay más tareas, se pone a "dormir" hasta que una nueva tarea llegue.

2.  **Scheduler**: Actúa como el controlador de tráfico del sistema. Recibe tareas de todas las fuentes (el código en ejecución o las APIs externas) y las enruta a la cola correcta. Es el responsable de decidir si una tarea es una Micro Tarea (ej. una promesa resuelta) o una Macro Tarea (ej. un evento de click, la respuesta de una API tradicional).

3.  **API Manager & API Workers**: Simula el "mundo exterior" (APIs del navegador como `fetch`, `DOM`, etc.).
    *   Cuando el código JS solicita una operación de I/O, el **API Manager** recibe la petición, guarda el contexto original de la tarea (que contiene la **referencia** al callback a ejecutar) y delega el trabajo a un **API Worker** (un hilo separado). Esta separación es clave: solo los datos esenciales viajan al "mundo exterior", protegiendo la lógica y el estado interno del motor.
    *   Una vez que el Worker completa su trabajo, notifica al API Manager, quien reconstruye la tarea con el resultado y la envía al **Scheduler** para que sea encolada.

4.  **Closure Heap**: Simula la memoria del motor donde se almacenan las "definiciones" de las funciones (Callbacks). Cuando se crea una tarea, no contiene el código en sí, sino una referencia (un ID) al callback almacenado en el Heap. Esto permite que el código persista en memoria, listo para ser ejecutado cuando una tarea asíncrona se complete, haciendo posible el concepto de *closures*.

5.  **Execution JS Stack**: Representado por la función `executeStackJS`, es la simulación del Call Stack de JavaScript. Se ejecuta en el hilo del **Event Loop** y procesa las instrucciones de un callback una por una. Desde aquí, la ejecución de una instrucción puede generar **nuevas tareas asíncronas** (como otra llamada a una API). Estas nuevas tareas son enviadas al **Scheduler** para ser procesadas, iniciando así un nuevo ciclo de trabajo asíncrono.

## ¿Cómo Funciona por Dentro? (La Implementación en C++)

El proyecto utiliza varias abstracciones de C++ para modelar el comportamiento del motor de forma segura y eficiente en un entorno concurrente.

*   **Arquitectura Multi-hilo**: El `main.cpp` lanza tres hilos principales que se ejecutan de forma concurrente:
    1.  `scheduler_thread`: El hilo del Scheduler.
    2.  `api_manager_thread`: El hilo que gestiona las operaciones de I/O.
    3.  `event_loop_thread`: El hilo que simula la ejecución single-threaded de JS.

*   **Comunicación Segura (`TaskQueue.h`)**: La comunicación entre hilos se realiza a través de colas seguras (`TaskQueue`). Esta clase envuelve una `std::deque` con un `std::mutex` para garantizar que las operaciones de inserción y extracción de tareas sean atómicas, evitando condiciones de carrera.

*   **Sincronización Eficiente (`Alarm.h`)**: Para evitar que los hilos consuman CPU innecesariamente mientras esperan tareas (busy-waiting), se utiliza la clase `Alarm`. Esta encapsula una `std::condition_variable` y permite que un hilo se "duerma" (`wait()`) de forma eficiente. La clave de su diseño es que el objeto `Alarm` de un hilo se comparte por referencia con aquellos otros hilos que necesitan despertarlo. Estos pueden llamarlo con `notify()` cuando han producido una nueva tarea, creando un modelo productor-consumidor muy eficiente.

*   **Simulación de Código JS (`Callback.h` y `ClosureHeap.h`)**:
    *   El "código JavaScript" se representa mediante la estructura `Callback`, que contiene un vector de `Instruction`. Cada `Instruction` representa una operación simple e individual (como `LOG` o `API_REQUEST`).
    *   El `ClosureHeap` actúa como un repositorio central (`std::map`) que asocia un `long long id` a cada `Callback`, simulando cómo la memoria del motor almacena las funciones.

*   **La Tarea como Mensaje (`Task.h`)**: La estructura `Task` es el mensaje que fluye por todo el sistema. Contiene toda la información necesaria para su procesamiento: su origen (`source`), su tipo (`is_promise`), el ID del callback a ejecutar (`callback_id`) y los datos asociados (`data`).

## Cómo Compilar y Ejecutar

Puedes compilar el proyecto utilizando un compilador de C++ compatible con C++17. Se requiere el flag `-pthread` para el manejo de hilos.

code
g++ main.cpp -o JSengine -std=c++17 -pthread
`

Una vez compilado, ejecuta el simulador:
code
.\JSengine.exe
`

El programa iniciará los hilos del motor, que se pondrán en estado de espera (`Going to sleep...`), y presentará un panel de control interactivo. Este panel te permitirá inyectar tareas en el sistema para observar su comportamiento en tiempo real, tal y como se ve en los logs.

### Opciones de Simulación:

1.  **Simular una promesa encadenada (`fetch().then()`):**
    *   **Qué hace:** Esta opción inyecta la *respuesta* de una API, simulando la resolución de una promesa inicial.
    *   **Qué observar en el log:** Verás cómo el **Scheduler** la identifica (`is_promise: true`) y la enruta directamente a la cola de **Micro Tareas**. El **Event Loop** la ejecuta de inmediato. Lo más interesante es que el código de esta primera microtarea genera una *nueva* petición a la API, que al resolverse, también es encolada como Micro Tarea y ejecutada.
    *   **El concepto clave:** Este flujo demuestra el **camino de alta prioridad** que siguen las promesas y cómo el sistema está diseñado para resolver operaciones encadenadas de forma consecutiva y predecible, lo cual es fundamental para manejar flujos de datos asíncronos.

2.  **Simular un evento de click del DOM (Macrotarea):**
    *   **Qué hace:** Esta opción simula un evento externo, como un click de usuario o la finalización de un `setTimeout`.
    *   **Qué observar en el log:** A diferencia de la anterior, el **Scheduler** identifica esta tarea como estándar (`is_promise: false`) y la enruta a la cola de **Macro Tareas**.
    *   **El concepto clave:** Esta simulación aísla y demuestra el **camino estándar** para los eventos generales. Aunque en esta prueba no compite con ninguna microtarea, ilustra el mecanismo por el cual se gestionan las interacciones del usuario y otras tareas asíncronas comunes. Representa el ciclo base del Event Loop, que por diseño, siempre daría prioridad a las microtareas antes de procesar una macrotarea.

## Estructura de Archivos

code
.
├── Alarm.h                 # Primitiva de sincronización para dormir/despertar hilos.
├── Callback.h              # Define las estructuras para simular código JS (Callback, Instruction).
├── ClosureHeap.h           # Simula la memoria del motor donde se guardan los callbacks.
├── main.cpp                # Punto de entrada. Lanza los hilos y contiene la lógica de cada componente.
├── Task.h                  # Define la estructura Task, el mensaje que fluye por el sistema.
├── TaskQueue.h             # Implementación de una cola genérica segura
`

