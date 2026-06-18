-module(job_sch).
-export([start_job/2, init_job/2]).

%% ===================================================================
%% API Pública
%% ===================================================================

%% @doc Inicia un proceso actor para un Job específico.
%% ResourceRequests es una lista de tuplas desordenadas con el formato:
%% [{{IP_Vector}, Puerto, TipoRecurso, Cantidad}]
%% Ejemplo: [{{192,168,1,11}, 8100, gpu, 1}, {{192,168,1,10}, 8100, cpu, 2}]
start_job(JobId, ResourceRequests) ->
    spawn(?MODULE, init_job, [JobId, ResourceRequests]).

%% ===================================================================
%% Funciones Internas del Actor
%% ===================================================================

%% @doc Inicializa el estado del Job y establece la invariante global.
init_job(JobId, ResourceRequests) ->
    %% Inicializamos la semilla aleatoria local para este proceso de forma única
    rand:seed(exsp),
    io:fwrite("[Job ~p] Inicializando simulación de carga.~n", [JobId]),
    
    %% Ordeno la lista de recursos
    %% Erlang ordena las tuplas elemento por elemento nativamente de izquierda a derecha:
    %% IP (Tupla de 4 enteros) -> Puerto (Entero) -> Tipo (Átomo: cpu < gpu < mem) -> Cantidad
    OrderedRequests = lists:sort(ResourceRequests),
    
    io:fwrite("[Job ~p] Recursos ordenados (Prevención de Deadlock):~n ~p~n", 
              [JobId, OrderedRequests]),
    
    %% Iniciamos el bucle de adquisición con 0 reintentos previos
    acquire_loop(JobId, OrderedRequests, [], OrderedRequests, 0).


%% @doc Bucle recursivo de asignación de recursos.
%% CASO BASE: Ya no quedan recursos pendientes en la cola.
acquire_loop(JobId, [], Acquired, _Original, _Attempts) ->
    io:fwrite("[Job ~p] ¡ÉXITO! Todos los recursos concedidos.~n", [JobId]),
    execute_payload(JobId, Acquired);

%% CASO RECURSIVO: Intentar adquirir el recurso que está a la cabeza de la lista ordenada.
acquire_loop(JobId, [NextResource | Rest], Acquired, Original, Attempts) ->
    io:fwrite("[Job ~p] Solicitando recurso: ~p~n", [JobId, NextResource]),
    
    %% Transformamos la tupla a ASCII
    AsciiMsg = format_request(JobId, NextResource),
    comms_interface ! {job_request, self(), JobId, AsciiMsg},
    
    receive
        {job_granted, JobId} -> 
            io:fwrite("[Job ~p] Concedido: ~p~n", [JobId, NextResource]),
            acquire_loop(JobId, Rest, [NextResource | Acquired], Original, Attempts);
            
        {job_denied, JobId} ->
            io:fwrite("[Job ~p] ALERTA: Recurso denegado (~p). Iniciando aborto...~n", 
                      [JobId, NextResource]),
            handle_failure(JobId, Acquired, Original, Attempts);
            
        {job_timeout, JobId} ->
            io:fwrite("[Job ~p] ALERTA: Timeout en red por recurso (~p).~n", 
                      [JobId, NextResource]),
            handle_failure(JobId, Acquired, Original, Attempts)
            
    after 10000 ->
        io:fwrite("[Job ~p] ERROR CRÍTICO: Timeout interno del planificador.~n", [JobId]),
        handle_failure(JobId, Acquired, Original, Attempts)
    end.


%% @doc Gestiona el fracaso restaurando la precondición (Rollback + Backoff)
handle_failure(JobId, Acquired, Original, Attempts) ->
    %% 1. Romper la condición de "Hold and Wait": Liberar de forma sincrónica lo obtenido
    release_all(JobId, Acquired),
    
    NextAttempts = Attempts + 1,
    %% 2. Evitar Livelock: Dormir al proceso un tiempo exponencial con aleatoriedad
    apply_backoff(JobId, NextAttempts),
    
    %% 3. Reiniciar la máquina de estados desde la lista original limpia
    io:fwrite("[Job ~p] Reiniciando ciclo de peticiones (Intento ~p).~n", [JobId, NextAttempts]),
    acquire_loop(JobId, Original, [], Original, NextAttempts).


%% @doc Libera recursivamente todos los recursos esperando confirmación para evitar carreras.
release_all(_JobId, []) ->
    ok;
release_all(JobId, [Resource | Rest]) ->
    io:fwrite("[Job ~p] Rollback/Liberación -> ~p~n", [JobId, Resource]),
    
    %% Formateamos la liberación según el protocolo ASCII del TP
    AsciiMsg = io_lib:format("JOB_RELEASE ~p\n", [JobId]),
    
    comms_interface ! {job_release, self(), JobId, AsciiMsg},
    
    receive
        {job_released, JobId} ->
            release_all(JobId, Rest)
    after 5000 ->
        io:fwrite("[Job ~p] Advertencia: Timeout esperando confirmación de liberación.~n", [JobId]),
        release_all(JobId, Rest)
    end.


%% @doc Simula la ejecución real del Job una vez bloqueados todos los recursos.
execute_payload(JobId, Acquired) ->
    io:fwrite("[Job ~p] Ejecutando procesamiento en el clúster simulado...~n", [JobId]),
    timer:sleep(5000),
    
    io:fwrite("[Job ~p] Procesamiento completado. Liberando infraestructura.~n", [JobId]),
    release_all(JobId, Acquired),
    io:fwrite("[Job ~p] Terminado correctamente.~n", [JobId]),
    ok.


%% @doc Calcula el Backoff Exponencial con Jitter (Truncado para evitar esperas infinitas)
apply_backoff(JobId, Attempts) ->
    BaseTime = 150, 
    CapAttempts = lists:min([Attempts, 5]), 
    
    ExpTime = BaseTime * trunc(math:pow(2, CapAttempts)),
    Jitter = rand:uniform(300), 
    
    TotalSleep = ExpTime + Jitter,
    io:fwrite("[Job ~p] Respetando backoff de ~p ms para mitigar contención.~n", 
              [JobId, TotalSleep]),
    timer:sleep(TotalSleep).


%% @doc Transforma la tupla del recurso al formato ASCII que espera el Agente C
format_request(JobId, {{IP1, IP2, IP3, IP4}, _Port, Type, Amount}) ->
    IpStr = io:fwrite("~p.~p.~p.~p", [IP1, IP2, IP3, IP4]),
    io:fwrite("JOB_REQUEST ~p @~s:~p:~p\n", [JobId, IpStr, Type, Amount]).