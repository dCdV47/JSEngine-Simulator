# JS MOTOR MODEL

## 0- Todo es gestionado por el motor del navegador/servidor Node.js (V8, SpiderMonkey)

## 1- API Manager - maneja las peticiones y respuestas API

Este motor está viendo el "mundo exterior" a través de las Web/C++ APIs.
Hay diferentes APIs con muchas funciones distintas conectadas y todas se encargan de avisar el motor que algo ha pasado en el mundo exterior.
Hay varias, para comprenderlas:

*   **API "DOM"** - se encarga de detectar los cambios en el DOM (clicks, pressKey...) y realizar cambios en el mismo.
*   **API "WebSocket"** - se encarga de abrir, cerrar y gestionar mensajes entrantes y salientes de sockets (on.message, send()...).
*   **API "IndexedDB"** - se encarga de comunicarse con la base de Datos interna del navegador, ya sea para recuperar o introducir datos.
*   **API "Tab Manager"** - se encarga de comunicarse con otras pestañas abiertas del navegador.
*   ...
*   **ESPECIAL:**
    *   **API "Renderizar"** - Pinta la pantalla de Chrome, Firefox, etc. No es una API como tal, es un programa aparte que corre en el navegador a 60fps y se sincroniza con los datos del EVENT LOOP.

### *SOBRE LAS LLAMADAS APIs, las MACROTASKS/MICROTASKS y las Promesas*

a) Cada vez que hacemos una petición a una API desde el código JS, cuando esta API devuelva los datos para que se ejecute un Callback, este Callback se encolará como una **MACROTASK**.

b) Somos nosotros, envolviendo estas llamadas en Promesas los que logramos que se encole como **MICROTASK** cuando se resuelva.

c) Hay 2 excepciones de APIs que sus respuestas van directamente a la cola de **MICROTASKS**:
*   `fetch()`: envuelve la petición HTTP en una Promesa para que se encole como **MICROTASK** cuando se reciba la respuesta.
*   `MutationObserver`: observa cambios en el DOM. Agrupa los cambios y ejecuta su callback como una **microtask** para asegurar que reaccionas a los cambios del DOM antes de que el navegador vuelva a pintar la pantalla.

## 2- En el Setup, el motor lo hace todo y crea diferentes elementos.

a) **CALLSTACK**: es donde ejecuto el codigo JS. Es un solo hilo. Meto código, lo ejecuto y, cuando acaba de ejecutarse, lo vacío. Una ejecución a la vez.

El codigo se ejecuta linea tras linea, hasta que:
1.  La función principal de la tarea termina (por return o fin de código).

2.  La función principal de la tarea es ASYNC y encuentra con un "await".
    En ese momento se crea una Promesa<pending/resuelta> y en su "funcionCallback" se guarda el código de la función hasta:
    *   el siguiente "await"
    *   el "return" de la función o fin de código

    **NOTA:** esta promesa casi siempre se crea como "pending". Podría crearse como "resuelta" en un caso como:

```javascript
function envolturaParaNoEjecutar(){
    //Llega un mensaje por el WebSocket (MACROTASK)
    webSocket.onmessage = async (data) => {

        const savedMessages = await saveMessagesToDB(data?.payload?.messages); 
        //se crea una Promesa<fullfilled> al instante porque no hay mensajes

        /*
        el resto del código se encolará como una MICROTASK al instante (la Promesa ya está resuelta)
        ...
        */
        return savedMessages;
    }
    }        
    function saveMessagesToDB(messages, storeName=MSG_STORE_NAME) {
    //entramos aquí, directamente se resuelve la promesa
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
    /* aquí directamente se resuelve la promesa, y se encola como MICROTASK el callback
```

3.  En cualquier otro caso, el código continuará.
    Por ejemplo: Se invoca una API asíncrona como `setTimeout`, `fetch`, etc.
    La llamada a la API en sí es síncrona, el callback se programa para más tarde y la ejecución del código actual continúa.

b) **COLA DE MACROTASK**: es donde pongo las "tareas" lanzadas directamente por los eventos de API Web/C++ (onClick, respuesta base de datos, etc).

c) **COLA DE MICROTASK**: es donde meto los "callback" de las Promesas Resueltas.

d) **RENDERIZADOR**: es donde guardo y modifico la información para renderizar el DOM.
* *Soy el guardian de la pantalla.*
* *Me sincronizo con la API DOM para decirle que se actualice y con la API Renderizar para decirle lo que tiene que mostrar.*

e) **MEMORIA/HEAP**: Es donde guardo las variables/objetos creadas por el JS en el CALLSTACK.
* *El guardian de los closures. Me sincronizo muy bien con el CALLSTACK para hacer funcionar los "closures JS" correctamente: le paso al motor JS el valor correcto en cada momento de cada variables aunque tengan el mismo nombre en el codigo JS. Soy el que permite la magia de los closures, apoyando enormemente a la fantasía del "multihilo JS".*
* *El guardián de las Promesas. Cuando JS crea una promesa, me encargo de guardarla con estado "pending" en memoria y pongo en su "funcionCallback" el codigo que se tiene que ejecutar cuando esta promesa se resuelva.*

f) **SCHELUDER**: Me encargo de organizar el orden de ejecuciones del EVENT LOOP.
* *Cuando una API Web/C++ resuelve una tarea (como la respuesta a una petición fetch) o me avisa de un evento (ej, el usuario hizo click en un botón), me encargo de asignar esa tarea a la COLA DE MICROTASKS O MACROTASKS.*
    1.  Si es una tarea normal (evento API, click, socket, etc): la encolo en la **COLA DE MACROTASKS**.
    2.  Si es una tarea surgida del CALLSTACK, envuelta como `Promise` o `await` (normalmente fetch, DB, etc):
        a) Aviso a MEMORIA de que esa Promesa se ha resuelto para que cambie el estado a "fullfilled" o "rejected".
        b) Recupero su "funcionCallback", y la encolo en la **COLA DE MICROTASKS**.

h) **EVENT LOOP**: Me encargo de gestionar la ejecución del CALLSTACK y de comunicarme con la API de renderizado.

z) ... estoy durmiendo, y algo me despierta:
*   una API me manda datos porque llegó un fetch anterior
*   porque un usuario hizo click en un botón
*   se cargó un script JS desde HTML...

*Podríamos resumirlo en: no tenía nada pendiente, pero ahora el SCHELUDER me ha asignado una MACROTASK o una MICROTASK nueva*

a) **if:** Hay tareas pendientes en **COLA DE MACROTASK**? HAGO 1, solo 1.
    => Paso la primera MACROTASK (la primera que entró, FIFO) de la COLA al CALLSTACK y ejecuto.

b) **while:** Mientras en **COLA DE MICROSTASK** haya tareas pendientes, las ejecuto una tras otra hasta vaciarlas.
    => Paso la primera MICROTASK (la primera que entró, FIFO) de la COLA al CALLSTACK y ejecuto.

c) Llamo al **RENDERIZADOR** (si es necesario).

d) Compruebo, y si no hay nada pendiente, a dormir...

### ESTA ES LA NOVEDAD

*   Con esto se logra dar prioridad a las MICROTASKS (crucial para gestionar concurrencia).
*   Esto permite que el encadenamiento de Promesas `.then()` o `await` (crucial para encadenar peticiones Web/C++ API prioritarias).

#### Ejemplo práctico: Servidor Node.js

1.  111 usuarios distintos nos hacen llegar una petición a través de la API WebSockets al mismo tiempo (aprox).

2.  **SCHELUDER** encola todas estas llamadas en la **COLA DE MACROTASK** por orden de llegada.

3.  **USUARIO 1**
    El **EVENT LOOP** ejecuta la primera macrotask en el **CALLSTACK**. Esta ejecución incluye una promesa que hace una petición a la API DB.
    a) Esta promesa queda en **MEMORIA** como "pending" y se le asigna su "función CallBack".
    b) La API DB se pone a trabajar por su cuenta en cumplir para recuperar los datos pedidos.

4.  El **CALLSTACK** se vacía después de ejecutar todo.

5.  El **EVENT LOOP** ve que **COLA DE MICROTASKS** está vacía, entonces renderiza, y empieza otra vez el bucle.

6.  ... esto ocurre con los 10 primeros usuarios igual.

7.  **USUARIO 11**: aquí cambia la cosa, mientras ejecuta la **MACROTASK** del USUARIO 11 en el **CALLSTACK**...
    **ESTÁN PASANDO COSAS EN EL MOTOR!!!**
    *   Las APIs DB han devuelto 3 resultados. Los datos que esperaban los usuarios 1, 3 y 7 se han cumplido.
    *   **SCHELUDER** coge estas Promesas que envolvían la llamada a las APIs DB, las marca como `fullfilled`, coge sus "funciones Callback" y las añade a la **COLA DE MICROTASKS**.

8.  Mientras **SCHELUDER** hacía su trabajo (de forma paralela, es totalmente independiente a la ejecución del CALLSTACK)...
    *   La ejecución de la **MACROTASK** del USUARIO 11 en el **CALLSTACK**, se terminó y el **CALLSTACK** ahora está vacío.
    *   Entonces, El **EVENT LOOP** mira y ve que **COLA DE MICROTASKS** ya tiene tareas!
        *   Va metiendo 1 por 1 en el **CALLSTACK** hasta que no queda ninguna.
        *   ***NOTA IMPORTANTE:*** *La MICROTASK del USUARIO 1 genera otra Promesa durante su ejecución.*

9.  Mientras se resolvía la **MICROTASK** del USUARIO 7, la API DB ha devuelto los datos del USUARIO 11 (acaba de llegar y ya los tiene, que suertudo!) y también los datos de la promesa. **SCHELUDER**, ni corto ni perezoso, marca las promesaS como cumplidas y añade sus "funciones Callback" a la **COLA DE MICROTASKS**.

10. El **EVENT LOOP** ve que **COLA DE MICROTASKS** está vacía, ya se ejecutaron todas:
    a) Las promesas iniciales de los usuarios 1, 3, 7, y 11 (el suertudo!).
    b) La promesa encadenada por la MICROTASK del USUARIO 1.

11. Ahora, y solo ahora, el **EVENT LOOP** pasa a la **cola de MACROTASK** para gestionar la siguiente petición, la del USUARIO 12.

> **Reflexión:** con esto que se logra? Pues que el usuario 1, 3, 7 y 11, ya están servidos!
> Si no existiera la COLA DE MICROTASKS, todas estas respuestas de la API DB se habrían asignado a la COLA DE MACROTASKS (como ocurría en el pasado, cuando la COLA DE MICROTASKS no existía) y todavía se tendría que gestionar la petición de 100 usuarios antes de acabar de resolver completamente las peticiones de los usuarios anteriores.
>
> Esto sería una experiencia de usuario malísima y un problema de concurrencia brutal. Node.js no tendría sentido.
>
> Sin embargo, con este "truquito", el motor de Node.js funciona como un servidor multihilo aunque haya solo un hilo de JS ejecutandose en el CALLSTACK.

**PD:** En el navegador sería lo mismo, antes de gestionar nuevos eventos del DOM, el motor comprueba si hay peticiones anteriores (promesas) que quedaron pendientes antes de meterse con tareas nuevas. Esto permite que todo sea más responsive.