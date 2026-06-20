%forma de mandarle los datos
% [IP:<ip>:PUERTO:<puerto>,CPU:<Ncpu>,MEM,<Nmem>,(GPU:<Ngpu> opcional)]

%NODES 192.168.1.10:8100:cpu:4:mem:8192:gpu:1;192.168.1.11:8101:cpu:2:mem:4096

-module(job_manager).
-include("job_manager.hrl").

-compile(export_all).
-define(PUERTOC, 3956).
-define(HOST, "localhost").
-define(CONFIGCONNECT, [{active,false},{packet,0}]).
-define(SEC, 1000).
-define(MAX_TIME_WORKING, 10 * ?SEC).
-define(MAX_TIME_ASKING_FOR_NODES, 50 * ?SEC).
-define(TIMEOUT, 1000 * ?SEC).
-define(TIME_BEFORE_DOING_MORE_JOBS, 10* ?SEC).

-record(recursos,{cpu,mem,gpu}).
-record(direccion,{ip,puerto}).

logFile(Msg) ->
    file:write_file("logErl.txt",[Msg ++ "\n"],[append]).

logFile(Msg,Args) ->
    Str = io_lib:format(Msg,Args),
    file:write_file("logErl.txt",[string:chomp(Str) ++ "\n"],[append]).


conectarse_a_nodo_local() ->
    case gen_tcp:connect(?HOST,?PUERTOC,?CONFIGCONNECT) of
        {ok,SockC} -> 
            logFile("Erlang se conecto al nodo local"),
            {ok,SockC};
        {error,Razon} -> 
            logFile("Error al conectarse al nodo, razon:~p ~n",[Razon]),
            exit(Razon) %handelear esto
    end.

manager_init() ->

    logFile("---Log del manager de erlang---"),
    {ok,SockC} = conectarse_a_nodo_local(),
    spawn(?MODULE,wait_recv,[SockC]),

    NodesPid = spawn(?MODULE,obtener_y_formatear_nodos,[]),
    register(get_nodes,NodesPid),


    JobMapPid = spawn(?MODULE,job_lista,[maps:new()]),
    register(job_map,JobMapPid),

    CommsPid = spawn(?MODULE,send_manager_f,[SockC]),
    register(send_manager,CommsPid),

    ManagerPid = spawn(?MODULE,manager,[]),
    register(manager,ManagerPid).

enviar_a_nodo(Msg) ->
    send_manager ! Msg.
format_nodes([Node]) -> 
    Map = maps:new(),
    case string:lexemes(Node,":") of
        [Ip,Puerto,"cpu",Cpu,"mem",Mem,"gpu",Gpu]->
            Recursos = #recursos{cpu = Cpu,mem = Mem,gpu = Gpu},
            Direccion = #direccion{ip = Ip, puerto = Puerto},
            maps:put(Direccion,Recursos,Map);

        [Ip,Puerto,"cpu",Cpu,"mem",Mem] ->
            Recursos = #recursos{cpu = Cpu,mem = Mem,gpu = 0},
            Direccion = #direccion{ip = Ip, puerto = Puerto},
            maps:put(Direccion,Recursos,Map);

        _ -> Map
    end;

format_nodes([Node | Nodes]) ->
    Map1 = format_nodes([Node]),
    Map2 = format_nodes(Nodes),
    maps:merge(Map1,Map2).

obtener_recursos_para_jobs(Nodes) ->
    todo,
    ["@192.168.1.2:cpu:2 @192.168.1.3:gpu:1","@192.168.1.2:cpu:2 @192.168.1.3:gpu:1"].
    %Dirs = maps:keys(Nodes),
    %NodeSelected = maps:get(rand:uniform(length(Dirs)),Nodes),
    %maps:find("",NodeSelected)

obtener_y_formatear_nodos() ->
    receive
        {ok,Response} -> 
            Nodes = string:lexemes(Response,";"),
            ParsedNodes = format_nodes(Nodes),
            manager ! {ok,ParsedNodes},
            obtener_y_formatear_nodos();

        {error,Razon} -> 
            logFile("Error al recibir la lista de nodos, razon:~p ~n",[Razon]),
            manager ! {error, Razon}
    after 
        ?TIMEOUT ->
            close %haceralgo
    end.


job_lista(JobMap) ->
    receive
        {find,JobAction, JobId} -> 
            JobPid = maps:get(JobId,JobMap),
            JobPid ! JobAction,
            job_lista(JobMap);
        {add,JobPid,JobId} -> 
            NewMap = maps:put(JobId,JobPid,JobMap),
            job_lista(NewMap);
        {remove,JobId} -> 
            NewMap = maps:remove(JobId,JobMap),
            case maps:size(NewMap) of
                0 -> manager ! alljobsdone;
                _ -> ok 
            end,    
            job_lista(NewMap)
    end.

wait_recv(SockC) ->
    case gen_tcp:recv(SockC, 0) of     
        {ok, "JOB_GRANTED "++ JobIdn} -> 
            JobId = list_to_integer(string:reverse(string:prefix(string:reverse(JobIdn),"\n"))),
            job_map ! {find,jobGranted, JobId},
            wait_recv(SockC);
        {ok, "JOB_DENIED "++ JobIdn} -> 
            JobId = list_to_integer(string:reverse(string:prefix(string:reverse(JobIdn),"\n"))),
            job_map ! {find,jobDenied, JobId},
            wait_recv(SockC);
        {ok, "JOB_TIMEOUT "++ JobIdn} -> 
            JobId = list_to_integer(string:reverse(string:prefix(string:reverse(JobIdn),"\n"))),
            job_map ! {find,jobTimeout, JobId},
            wait_recv(SockC);
        {ok,"NODES " ++ Nodesn} -> 
            Nodes = string:reverse(string:prefix(string:reverse(Nodesn),"\n")),
            get_nodes ! {ok, Nodes},
            wait_recv(SockC);
        {error,Razon} -> 
            logFile("Error al recibir mensaje del nodo, razon:~p ~n",[Razon]),
            {error,Razon};

        _ -> wait_recv(SockC)
    end.


send_manager_f(SockC) ->
    receive
        {jobRelease,JobId} -> 
            JOB_RELEASE = "JOB_RELEASE " ++ integer_to_list(JobId) ++ "\n",
            ok = gen_tcp:send(SockC, JOB_RELEASE);
        {jobRequest, JobId, Recursos} ->
            JOB_REQUEST = "JOB_REQUEST " ++ integer_to_list(JobId) ++ " " ++ Recursos ++ "\n",
            ok = gen_tcp:send(SockC, JOB_REQUEST);
        getNodes -> 
            ok = gen_tcp:send(SockC, "GET_NODES\n");
        close -> close %haceralgo
    end,
    send_manager_f(SockC).

crear_job(Resource) -> 
    JobId = erlang:unique_integer([positive]),
    JobPid = spawn(?MODULE,acquire_loop,[JobId,Resource,0]),
    job_map ! {add,JobPid,JobId}.

handle_nodes() ->
    send_manager ! getNodes,
    receive
        {ok,Nodos} -> Nodos;
        {error,Razon} -> 
            logFile("No se pudo obtener los nodos, razon:~p ~n",[Razon]),
            {error,Razon}
    end.

manager() ->
    MapaNodos = handle_nodes(),    
    logFile("nodos: ~p~n", [MapaNodos]),
    Recursos = obtener_recursos_para_jobs(MapaNodos),
    lists:foreach(fun(Recurso) -> crear_job(Recurso) end,Recursos),
    receive
        alljobsdone -> ok
    end,
    timer:sleep(?TIME_BEFORE_DOING_MORE_JOBS),
    manager().


execute_payload(JobId) ->
    logFile("[Job ~p] Ejecutando procesamiento en el clúster simulado...~n", [JobId]),
    timer:sleep(5 * ?SEC),
    
    logFile("[Job ~p] Procesamiento completado. Liberando infraestructura.~n", [JobId]),
    send_manager ! {jobRelease,JobId},
    job_map ! {remove,JobId},
    logFile("[Job ~p] Terminado correctamente.~n", [JobId]),
    ok.

acquire_loop(JobId, Recursos, Intentos) ->
    logFile("[Job ~p] Solicitando los recursos: ~p~n", [JobId, Recursos]),
    
    send_manager ! {jobRequest, JobId, Recursos},
    
    receive
        jobGranted -> 
           logFile("[Job ~p] ¡ÉXITO! Todos los recursos concedidos.~n", [JobId]),
            execute_payload(JobId);
            
        jobDenied ->
            logFile("[Job ~p] ALERTA: job denegado (~p). Iniciando aborto...~n", [JobId, Recursos]),
            handle_failure(JobId, Recursos, Intentos);
            
        jobTimeout ->
            logFile("[Job ~p] ALERTA: Timeout en red por recursos (~p).~n", [JobId, Recursos]),
            handle_failure(JobId, Recursos, Intentos)
            
    after ?TIMEOUT ->
        logFile("[Job ~p] ERROR CRÍTICO: Timeout interno del planificador.~n", [JobId]),
        handle_failure(JobId, Recursos, Intentos)
    end.

handle_failure(JobId, Recursos, Intentos) ->
    SiguienteIntento = Intentos + 1,
    %% 2. Evitar Livelock: Dormir al proceso un tiempo exponencial con aleatoriedad
    aplicar_timeout(JobId, SiguienteIntento),
    
    %% 3. Reiniciar la máquina de estados desde la lista original limpia
    logFile("[Job ~p] Reiniciando ciclo de peticiones (Intento ~p).~n", [JobId, SiguienteIntento]),
    acquire_loop(JobId, Recursos, SiguienteIntento).



aplicar_timeout(JobId, Intentos) ->
    TiempoBase = 150, 
    Limitar = lists:min([Intentos, 5]), 
    
    TiempoExponensial = TiempoBase * trunc(math:pow(2, Limitar)),
    Jitter = rand:uniform(300), 
    
    TiempoTotalSleep = TiempoExponensial + Jitter,
    logFile("[Job ~p] Respetando backoff de ~p ms para mitigar contención.~n", [JobId, TiempoTotalSleep]),
    timer:sleep(TiempoTotalSleep).
