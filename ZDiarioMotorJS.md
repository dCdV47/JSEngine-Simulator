Perfecto. Mira, sé que no es lo ortodoxo, pero no quiero un proceso de organización del todo, tal y como podríamos hacer, sino una iteración similar a lo que ha pasado con JS.

Entonces, tenemos primero que definir la base basiquisima (no desde el inicio de los tiempos porque ahi debía ser scripts y ya).
1- Lugar donde se va a ejecutar, nuestro, STACK DE EJECUCIÓN, o mejor dicho por simplificación, nuestro EVENT LOOP, esperando permanentemente por una nueva tarea que hacer, para, en este caso, simplemente imprimirla.
2- API que va a detectar interecciones del usuario (la vamos a simular si es que dificil porque es simplemente una cosa tanto).
3- Scheduler y Cola de macrotask: quien y donde se van a encolar TODOS los callbacks. Al pricnipio era así, una sola cola gestionaba todo. Así vamos a empezar. Luego, ya veremos qué magia hacemos para introducir las promises, pero en principio vamos a hacer esto. Plafinicarlo todo desde el principio es trampa, luego tendremos que encontrar la forma de resolverlo sin destruirlo todo.


Tenemos que empezar, seguro seguro, y no quiero pensar en nada más, en diseñar nuestra Queue (esta clase luego se va a usar las dos colas).

Las caracterustucas de esta cola tienen que ser las siguientes:
1- Tiene que ser un vector que acepte cualquier cosa que le llegue (por flexibilidad futura)

2- Tiene que poder definir un productor, mejor dicho escritor, porque las task las producen otros, que va a ser el scheduler pasándole las tasks

3- Tiene que poder definir un lector, que es el EVENT LOOP que va a sacar la tarea para ejecutarla. Después de leer una tarea (solo se podrá leer la primera), esa tarea tiene que ser borrada.

Sé que se podría simplificar, haciendo que el event loop solo leyera y que el scheduler solo escribiera, pero quiero hacer esa cosa de seguridad si no es extremadamente dificil.

¿Cómo hacer esto en este clase?

Pues un atributo vector Template (o otra clase que consideres mejor, pero que funcione así, que sea facil hacerle "array_shift" de PHP)

Una varaible que guarde la referencia del hilo escritor
Y otro puntero que guarde la referencia del hilo productor

Una función de "escritura" (meter algo en la parte final del vector)

Una función de "lectura" (sacar algo de la parte incial del vector)

Por supuesto, creo que para no generar conflicto, ambas funciones necesitan un mutex que proteja el código, para que o se lea o se escriba, pero no las dos a la vez.

Luego, por supuesto, podríamos poner una función de depuración de "imprime todo el vector", pero ya vamos viendo. Creo que esta base es sólida contando que podamos definir mi truco loco de la referencia del hilo.

Es que quiero copiar un poco el "promise/future" de C++.

Nuestro schudeler, que va a ser el hilo principal. Creará la queue en el STACK, pero con el vector e identificadores en el HEAP. Esto debería asignarle como "escritor" automáticamente, en el propio constructor.

Luego, se la pasará el EVENT LOOP que se va a crear en otro hilo. Aquí, quizás en la primera lectura o con una función especial de "getReadingRights", que solo se pudiera llamar si aún no tiene lector asignado, se asignará a este hilo como el lector. Y se acabó. (entiendo que promise/future usa dos objetos, para luego poder compartir lecturas, dos objetos que se coordinan entre ellos y tal y cual, pero no es el caso de esta cola y creo que no es necesario tanto. Podríamos hacer también que en el momento de "construcción por copia", al pasarse al otro hilo, directamente se le asignara como lector, pero lo veo mucho oculto y dificil de depurar)

Antes de escribir código, qué te parece?



Lo vamos a dejar tranquilito así, sin condition variable, solo queremos que vigile el acceso. Lo otro lo vamos a controlar de otra manera.

Te cuento, todo se va a manejar un principio en 3 hilos principales, y otros N hilos que utilizaremos para simular las llamadas y respuestas a APIs, pero eso no es lo importante ahora.

HILO MOTOR (el MAIN y que lanza el resto de hilos): el SCHEDULER. 
    Contendrá una cola de la clase que hemos creado llamada ALL_TASKS

    Esta cola tendrá 2 escritores distintos: 
        • los hilos de API escribirán aquí las respuestas/llamadas del exterior, que el SCHUDELER pasara a nuestro event loop asignandoselo a su cola de macrotasks/microtasks
        • nuestro hilo EVENT LOOP escribirá aquí también. Tendrá una función llamada execute_js_stack (o algo así), que podrá asignar tareas al SCHEDULER de "heyyy, haz esta llamada", y el SCHEDULER se encargará de pasar la llamada al API MANAGER (un API manager, que, por simplificación y claridad en el dominio del sistema, creará un nuevo hilo que representará la llamada fetch a otro servidor, el cambio del DOM a aplicar, lo que sea...)

    • Contendrá, aparte, una condition variable de espera para dormir. Puede ser un objeto ALARM que será llamado después de que se meta una tarea en su cola. Lo que comprobará antes de irse a dormir es si su hay tareas pendientes en su cola. Este objeto ALARM se lo pasará por referencia tanto al hilo de EVENT LOOP como al despertador de la API


HILO EVENT LOOP
    Contendrá el event loop (obvio)

    Dos colas: macrotask y microtask
        Un único escritor, el Scheduler, que le asignará tareas

    Un despertador para ser usado por SCHEDULER después de asignar una tarea

    La función execute_stack_js, que finjirá ejecuciones de JS, y que podrá encolar tasks de peticiones al scheduler.

HILO API MANAGER
    Muy sencillo, contendrá una sola COLA también, API Tasks, en esta cola escribirá el SCHEDULER para asignar tareas y las APIs para devolverlas

    Un despertador, que podrá ser usado por el SCHEDULER y los hilos que cree reprensentado "respuestas API"


OBJETO TASK
    Súper importante, será un único objeto para TODO. Osea una task va a ser una task en cualquier parte del flujo, pero en función de sus flags y etiquetas de se comportará de manera diferente. Es la idea de imitar también un poco el funcionamiento de los sockets en un servidor de sockets, realmente las peticiones llegan por un solo hilo y son gestionadas en una cola. Este objeto task tiene que tener diferentes cosas dentro de sí, campos:
        UN ID_unico
        JEFE: (el que asignó la tarea, hay que buscar un nombre mejor): JS_STACK, SCHEDULER, API
        ACCION: request/respond
        IS_PROMISE: boolean
        CALLBACK: JS que se va a ejecutar en el STACK (esto lo mejoraremos en el futuro porque no se puede andar mandando esto a APIs externas por una cuestión de seguridad, y en este caso para simplificar va a completar todo el viaje)
        DATA: aquí va la petición/respuesta 



Te insisto, hazme caso: la cola solo va a comprobar que no sea lea y se escriba a la vez.
NADA más. Ya verás como queda todo muy bonito, confía en mí.

Las esperas las vamos a hacer a través de despertadores, que de forma desacoplada los otros hilos decidirán cuando despertar.

Necesitamso una clase despertador al que se le pueda hacer notify y que haga que los hilos se queden esperando FUERA de los bucles. Una vez que han vaciado su cola/colas (en el caso del event loop que tiene que hacerlo en dos), ahí un conditión variable nos va a ayudar a despertar, CUANDO LO DIGA EL HILO EXTERNO. Esta es la arquitectura, y no puede fallar, lo vas a entender más adelante en cuánto lo implementemos. No pierdas tiempo en pensar eso, centrate en implementar lo que te pido.

Primero, quiero que implementes esta clase ALARM, vamos allá, no le des muchas vueltas a las posibles fallas lógicas que puedes estar encontrado, te acabo de dar un esquemita simple, yo tengo la idea mucho más clara, y la vas a ver muy pronto. Hazme la ALARM.

Necesitamos un detalle muy importante: nuestra ALARM debe recibir la condición a evaluar en la condition_variable en el construtor de la misma. Así la hacemos flexible y desacoplada.


Vale, lo que quiero que hagas ahora, es que crees la estrucutra del hilo principal que te dije. Implementará todo, entonces:

1- Crear 3 alarmas:
    • La suya
    • La del EVENT LOOP
    • La del API manager

2- Crear 4 colas
    • La suya
    • La del API Manager
    • Las dos del eventLoop

3- Lanzar el API Manager en otro hilo, pasándole SU despertador y el suyo propio. Ahora se van a poder despertar mutuamente. También le pasará su cola y la de los otros (esto a través de una lamda). La comprobación para irse a dormir es: tengo tareas???? ya, nada más. Hará una función de "executeAPITask" que definiremos luego haciendole pop a la parte de adelante la cola mientras la cola no esté vacía, y luego se irá a dormir.

4- Lo mismo con event loop, pero un poco diferente. Aquí hay que pasarle dos colas propias en vez de 1. Y luego, el bucle es: si macrotasks no está vacío, hago pop a la primera y executeStackJS, luego con microtask pues se itera hasta que la cola esté vacía, y luego comprueba, y si las dos colas están vacías, duerme. Ese es el loop.

Haz solo esto que te digo, este código en main(), ya luego entramos a más detalles, no intentes hacerlo todo de golpe, solo lo que te he dicho en esta instruccion.


Vale, ahora vamos a definir la clase task.

Quiero que tenga: 
id_task (general)
ACCION: request/respond
source: de la task
id_callback (se lo asociaremos creando un map)
type: request
is_promise
data


bool is_promise;       // Flag para indicar si la tarea está asociada a una promesa. 
TaskType type;         // Determina si va a la cola de macrotareas o microtareas.
Esto y esto son equivalentes: si es promesa irá a microtareas cuando le toque, sino, pues no. Nos quedamos solo con is_promise para que sea más claro de ver. No modifiques el código, ya lo hago yo.


Vale, vamos a definir callback.
Se me ocurre definirlo como un vector de strings simplemente (cada string representa una linea). Luego podemos recorrerlo, y hacer (si esto es facil en C++) que si incluye "<<callPromise>>" o cosas así, osea lo que se quiera, que realice otras acciones. Aí la ejecución sería recorrer u nvector de strings 1 por 1, y realizar diferentes acciones si se encuentra una cadena detemirnada




Perfecto, ahora lo que quiero es en el API Manager es no mandar el id del callback fuera ni nada.

Yo pensaba hacerlo sencillito, sin crear una nueva clase, simplemente ahí dentro de la lamda del api manager (como mucho un struct de API request)

0- Crear una nueva cola para respuestas API
1- Registrar la tarea en una tabla hash cuando llega.
2- A la simulación de API, este hilo nuevo que vamos a crear, mandarle solo:
    struct: api request
    • ID task
    • Type: request
    • Data: los datos pedidos

    Aparte:
    • Despertador

3- Que la simulación de API devuelva, id task, type: respond, data: los datos devueltos, y despierte.

4- Y luego ahí ya coger esta respuesta en la cola de API; sacar el id para sacar la tarea de la tabla hash. Cambiarle la data con la respuesta que se da, y pumm, ya se lo metemos al scheluder.


Así quedó, ahora quiero implementar:
sendAPIRrequest(task); (se tiene que lanzar en otro hilo, simula)

lo que tiene que hacer antes de lanzarlo es
0- crear una struct ApiRequest {
    long long task_id;
    TaskAction action = TaskAction::REQUEST;
    std::any data;

1- pasarle a sendAPIRrequest la ApiRequest, la cola api_manager_response_queue, el despertador también

2- Finjir una espera, y al acabar, meter una ApiResponse en la cola del API Manager, y destarlo.

No te lies cambiando otras cosa: solo cambiame el sitio de la invocación y creame la función, ya si hay fallos los arreglo yo.


Perfecto, ya tenemos entonces API Manager completo.

Vamos a meternos ahora con la última parte más complicada: la simulación del stack.

    // 4. Lanzar el Hilo del Event Loop
    std::thread event_loop_thread([&]() {
        std::cout << "[EventLoop]: Hilo iniciado." << std::endl;
        while (true) { // El bucle de vida del EventLoop
            
            // Fase 1: Procesar UNA macrotarea (si existe)
            if (!event_loop_macrotask_queue.isEmpty()) {
                Task macro_task = event_loop_macrotask_queue.pop();
                executeStackJS(macro_task);
            }
            
            // Fase 2: Procesar TODAS las microtareas (hasta que la cola esté vacía)
            while (!event_loop_microtask_queue.isEmpty()) {
                Task micro_task = event_loop_microtask_queue.pop();
                executeStackJS(micro_task);
            }

            // Fase 3: Comprobar si hay que dormir
            if (event_loop_macrotask_queue.isEmpty() && event_loop_microtask_queue.isEmpty()) {
                std::cout << "[EventLoop]: No hay más tareas. Durmiendo..." << std::endl;
                event_loop_alarm.wait();
                std::cout << "[EventLoop]: Despertado por una notificación." << std::endl;
            }
        }
    });


0- He cambiado esto así, para que finjamos que nuestro "CallBack Manager" es la memoria, el guardián de los closures. Realmente no hará nada ni usaremos nada, es solo un número, pero es simbólico, no complica y explica un poco más o deja espacio a diseñar eso.
struct Callback {
    long long id;
    long long associated_closure;
    std::vector<Instruction> instructions;
};


YO diría, que en el event Loop tenemos que crear primero el:

1- CallbackManager, esto se lo pasaremos a la función "executeStackJS" para que registre los callbacks ahí (nuestro CallbackManager hará las funciones de memoria, guardando callbacks y closures)

2- Luego ya meternos con "executeStackJS". Esto debe ejecutarse en el mismo hilo, porque no es multihilo el stack, corre uno solo en el Event loop. Solo debe recibir el callback (como valor), la data de la task (como valor) y callback manager (como referencia).

3- Dentro, lo que hará, será imprimir la data "he recibido estos Datos!" y luego imprimir una por una el payload de las las instrucciones. SI alguna instrucción es:

struct Instruction {
    InstructionType type;
    std::string payload; // Datos para la instrucción (el mensaje a loguear, la URL de la API, etc.)
    bool is_api_request; //añadi esto
    bool is_promise;     // Es 'true' si esta instrucción (ej. API_REQUEST) debe ser tratada como una promesa
};

is_api_request, pues esto generará dos cosas:
    1- Primero, creará un callback de una sola instrucción, lo que ponga en el payload escrito, y que no será API resuqest ni is_promise.
    2- Se registrará este callback
    3- Se creará una tarea y se le insertara a nuestro scheluder.
    4- Se despertará al scheduler.


De momento voy a dejarlo el API Manager. La simulación lo que haremos será pasarle una tarea el scheduler lista, como si se la pasara el API Manager.

Así que vamos a ello, a desarrollar el loop del scheduler.

Igual, estoy pensando en crear otro hilo para el scheduler y así dejar el hilo principal como un loop en el que meter datos y cosas por consola. Igual un menú de opciones o así para probar. Algo así como si el loop principal fuera el del usuario funcionando. Es una capa de abstración más, pero es que creo que la mejor manera de ser un ente externo al sistema y poder meterle cosas.

Bueno, como quiero que funcione es:

1- reviso la cola de tareas (si está vacía a dormir)

2- Si hay algo, la gestiono. Fácil:
    si source = API WORKER -> pum, la meto en la cola de microtask o macrotask en función de is_promise, despierto al event loop
    si source =  EVENT_LOOP -> pum, se la meto al API Manager, despierto al API Mananger

Ahora mismo solo haz esto, ya luego hacemos el menú interactivo vale? AHora centrate solo en el hilo del SCHEDULER.


Perfecto, listo. Ahora vmaos a probar.

De momento sin opciones ni menu interactivo ni nada. Crea una tarea sencilla que solo haga log (sin que se asigne y haya nuevas respuestas ni nada). A ver si tenemos funcionando bien el event loop y el scheduler.

Vale perfecto, ahora vamos a añadir, que en vez de un callback genereico, se puedan encadenar promesa. Esto lo definimos al registrar un ID de Callback.

struct Instruction {
    InstructionType type;
    std::string payload; // Datos para la instrucción (el mensaje a loguear, la URL de la API, etc.)
    bool is_api_request;
    bool is_promise;     // Es 'true' si esta instrucción (ej. API_REQUEST) debe ser tratada como una promesa
    long long then_callback_id = -1; // -1 indica que no hay callback adjunto
};

Con modificar en excuteStack donde sucede es suficiente

Perfecto, ahora hacemos una simulación para simular un fetch.then

Se me ocurre: Ahora que tenemos esto montado. Vamos a añadir otra prueba. Esta prueba que simule lo siguiente porfa:

Inyectamos una tarea directamente en el API Manager. Lo que quiero es que simule una interacción del DOM. Se entiende no? Se supone que ya tenemos un callback definido (lo definimos), y un API WORKER nos avisa "ALGO HA PASADO". No podemos fingir con facilidad eso del API WORKER, entonces lo que hacemos es inyectarle esa tarea directamente en la cola del API Manager.

Tiene que meterse en el API MANAGER, porque es el que recibe lois mensajes de las APIs del exterior. Va a funcionar perfectamente, el se encargará de pasarselo al SCHEDULER.

Vale sí, una API realmente devuelve una DATA, pero como no podemos fingir todo eso porque es un HILO, se lo metemos en la cola con source API WORKER como lo otro y le despertamos y sabrá que hacer con él. Así es mucho más realista.

Así que para fingir el ciclo completo haremos lo siguiente:
INYECTAMOS EN EL SCHEDULER