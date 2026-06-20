%forma de mandarle los datos
% [IP:<ip>:PUERTO:<puerto>,CPU:<Ncpu>,MEM,<Nmem>,(GPU:<Ngpu> opcional)]

-module(job_manager).

-compile(export_all).

-define(PUERTOC, 3954).
-define(HOST, "localhost").
-define(CONFIGCONNECT, [{active,false},{packet,0}]).
-define(SEC, 1000).
-define(MAX_TIME_WORKING, 10 * ?SEC).
-define(MAX_TIME_ASKING_FOR_NODES, 50 * ?SEC).
-define(TIMEOUT, 1000 * ?SEC).
-define(TIME_BEFORE_DOING_MORE_JOBS, 10* ?SEC).

-record(recursos,{cpu,mem,gpu}).
-record(direccion,{ip,puerto}).

%[{{192.168.1.10,8100},[{cpu:1},{mem:2},{gpu:3}]}]
%NODES 192.168.1.10:8100:cpu:4:mem:8192:gpu:1;192.168.1.11:8101:cpu:2:mem:4096
%NODES [["192.168.1.10:8100:cpu:4:mem:8192:gpu:1"],["192.168.1.11:8101:cpu:2:mem:4096"]]
%S = string:lexemes("192.168.1.10:8100:cpu:4:mem:8192:gpu:1;192.168.1.11:8101:cpu:2:mem:4096",";").
connect_to_local_node() ->
    case gen_tcp:connect(?HOST,?PUERTOC,?CONFIGCONNECT) of
        {ok,SockC} -> {ok,SockC};
        {error,Reason} -> io:fwrite("Error al conectarse al nodo, razon:~p ~n",[Reason]),exit(Reason) %handelear esto
    end.

manager_init() ->
    {ok,SockC} = connect_to_local_node(),

    NodesPid = spawn(?MODULE,get_formated_nodes,[]),
    register(get_nodes,NodesPid),

    spawn(?MODULE,wait_recv,[SockC]),

    JobMapPid = spawn(?MODULE,job_lista,[maps:new()]),
    register(job_map,JobMapPid),

    CommsPid = spawn(?MODULE,comms_manager_f,[SockC]),
    register(comms_manager,CommsPid),

    ManagerPid = spawn(?MODULE,manager,[]),
    register(manager,ManagerPid).

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

get_random_resources(Nodes) ->
    ["@192.168.1.2:cpu:2 @192.168.1.3:gpu:1","@192.168.1.2:cpu:2 @192.168.1.3:gpu:1"].
    %Dirs = maps:keys(Nodes),
    %NodeSelected = maps:get(rand:uniform(length(Dirs)),Nodes),
    %maps:find("",NodeSelected)

get_formated_nodes() ->
    receive
        {ok,Response} -> 
            Nodes = string:lexemes(Response,";"),
            ParsedNodes = format_nodes(Nodes),
            manager ! {ok,ParsedNodes};

        {error,Reason} -> 
            io:fwrite("Error al recibir la lista de nodos, razon:~p ~n",[Reason]),
            manager ! {error, Reason}
    after 
        ?TIMEOUT ->
            close %haceralgo
    end.

logFile({com,Msg}) ->
    io:fwrite("~p~n",[Msg]),
    file:write_file("logErl.txt",Msg,append);
logFile({warn,Msg}) ->
    io:fwrite("Warning: ~p~n",[Msg]),
    file:write_file("logErl.txt",Msg,append);
logFile({fatal,Msg,SockC}) ->
    io:fwrite("Fatal: ~p~n",[Msg]),
    file:write_file("logErl.txt",Msg,append),
    gen_tcp:close(SockC),
    exit(Msg).

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
        {error,Reason} -> {error,Reason};

        _ -> wait_recv(SockC)
    end.


comms_manager_f(SockC) ->
    receive
        {jobRelease,JobId} -> 
            JOB_RELEASE = "JOB_RELEASE " ++ integer_to_list(JobId) ++ "\n",
            ok = gen_tcp:send(SockC, JOB_RELEASE);
        {jobRequest, JobId, Resources} ->
            JOB_REQUEST = "JOB_REQUEST " ++ integer_to_list(JobId) ++ " " ++ Resources ++ "\n",
            ok = gen_tcp:send(SockC, JOB_REQUEST);
        getNodes -> 
            ok = gen_tcp:send(SockC, "GET_NODES\n");
        close -> close %haceralgo
    end,
comms_manager_f(SockC).

create_job(Resource) -> 
    JobId = erlang:unique_integer([positive]),
    JobPid = spawn(?MODULE,acquire_loop,[JobId,Resource,0]),
    job_map ! {add,JobPid,JobId}.

handle_nodes() ->
    comms_manager ! getNodes,
    receive
        {ok,Nodos} -> Nodos;
        {error,Reason} -> fuck
    end.

manager() ->
    FormatedNodes = handle_nodes(),    
    io:fwrite("nodos: ~p~n", [FormatedNodes]),
    ListOfList = get_random_resources(FormatedNodes),
    lists:foreach(fun(Resource) -> create_job(Resource) end,ListOfList),
    receive
        alljobsdone -> ok
    end,
    timer:sleep(?TIME_BEFORE_DOING_MORE_JOBS),
    manager().


execute_payload(JobId) ->
    io:fwrite("[Job ~p] Ejecutando procesamiento en el clúster simulado...~n", [JobId]),
    timer:sleep(5 * ?SEC),
    
    io:fwrite("[Job ~p] Procesamiento completado. Liberando infraestructura.~n", [JobId]),
    comms_manager ! {jobRelease,JobId},
    job_map ! {remove,JobId},
    io:fwrite("[Job ~p] Terminado correctamente.~n", [JobId]),
    ok.

acquire_loop(JobId, Resources, Attempts) ->
    io:fwrite("[Job ~p] Solicitando los recursos: ~p~n", [JobId, Resources]),
    
    comms_manager ! {jobRequest, JobId, Resources},
    
    receive
        jobGranted -> 
           io:fwrite("[Job ~p] ¡ÉXITO! Todos los recursos concedidos.~n", [JobId]),
            execute_payload(JobId);
            
        jobDenied ->
            io:fwrite("[Job ~p] ALERTA: job denegado (~p). Iniciando aborto...~n", 
                      [JobId, Resources]),
            handle_failure(JobId, Resources, Attempts);
            
        jobTimeout ->
            io:fwrite("[Job ~p] ALERTA: Timeout en red por recursos (~p).~n", 
                      [JobId, Resources]),
            handle_failure(JobId, Resources, Attempts)
            
    after ?TIMEOUT ->
        io:fwrite("[Job ~p] ERROR CRÍTICO: Timeout interno del planificador.~n", [JobId]),
        handle_failure(JobId, Resources, Attempts)
    end.

handle_failure(JobId, Resources, Attempts) ->
    NextAttempts = Attempts + 1,
    %% 2. Evitar Livelock: Dormir al proceso un tiempo exponencial con aleatoriedad
    apply_backoff(JobId, NextAttempts),
    
    %% 3. Reiniciar la máquina de estados desde la lista original limpia
    io:fwrite("[Job ~p] Reiniciando ciclo de peticiones (Intento ~p).~n", [JobId, NextAttempts]),
    acquire_loop(JobId, Resources, NextAttempts).



apply_backoff(JobId, Attempts) ->
    BaseTime = 150, 
    CapAttempts = lists:min([Attempts, 5]), 
    
    ExpTime = BaseTime * trunc(math:pow(2, CapAttempts)),
    Jitter = rand:uniform(300), 
    
    TotalSleep = ExpTime + Jitter,
    io:fwrite("[Job ~p] Respetando backoff de ~p ms para mitigar contención.~n", 
              [JobId, TotalSleep]),
    timer:sleep(TotalSleep).
