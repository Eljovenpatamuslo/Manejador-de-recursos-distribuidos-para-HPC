%forma de mandarle los datos
% [IP:<ip>:PUERTO:<puerto>,CPU:<Ncpu>,MEM,<Nmem>,(GPU:<Ngpu> opcional)]

%NODES 192.168.1.10:8100:cpu:4:mem:8192:gpu:1;192.168.1.11:8101:cpu:2:mem:4096

-module(job_scheduler).

-export([scheduler_init/1,format_nodes/1,
    obtener_y_formatear_nodos/0,scheduler/0,get_pid_scheduler/0]).
-define(SEC, 1000).
-define(TIME_BEFORE_DOING_MORE_JOBS, 10* ?SEC).

-record(recursos,{cpu,mem,gpu}).
-record(direccion,{ip,puerto}).

scheduler_init(PuertoC) ->
    ok = logF:crear_error_managment(),
    logF:log(msg,"send_recv_manager iniciado ~n"),
    
    ok = send_recv_manager:send_recv_init(PuertoC),
    logF:log(msg,"send_recv_manager iniciado ~n"),

    ok = job_server:iniciar_job_server(),
    logF:log(msg,"job_server iniciado~n"),

    register(scheduler,self()),
    scheduler().

format_nodes([Node]) -> 
    case string:lexemes(Node,":") of
        [Ip,Puerto,"cpu",Cpu,"mem",Mem,"gpu",Gpu]->
            Recursos = #recursos{cpu = list_to_integer(Cpu),mem = list_to_integer(Mem),gpu = list_to_integer(Gpu)},
            Direccion = #direccion{ip = Ip, puerto = Puerto},
            [{Direccion,Recursos}];

        [Ip,Puerto,"cpu",Cpu,"mem",Mem] ->
            Recursos = #recursos{cpu = list_to_integer(Cpu),mem = list_to_integer(Mem),gpu = 0},
            Direccion = #direccion{ip = Ip, puerto = Puerto},
            [{Direccion,Recursos}];

        _ -> logF:log("Error: el orden de la solicitud de recursos no es compatible")
        
    end;

format_nodes([Node | Nodes]) ->
    List1 = format_nodes([Node]),
    List2 = format_nodes(Nodes),
    lists:append(List1,List2).

obtener_y_formatear_nodos() ->
    case send_recv_manager:obtener_nodos() of
        {ok,Response} -> 
            Nodes = string:lexemes(Response,";"),
            ParsedNodes = format_nodes(Nodes),
            ParsedNodes;

        {error,Razon} -> 
            logF:log("Error al recibir la lista de nodos, razon:~p ~n",[Razon]),
            {error, Razon}
    end.


scheduler() ->
    ListaNodos = obtener_y_formatear_nodos(),    
    logF:log("nodos: ~p~n", [ListaNodos]),
    Recursos = job_generator:obtener_recursos_para_jobs(ListaNodos),
    lists:foreach(fun(Recurso) -> job_server:crear_job(Recurso) end,Recursos),
    receive
        alljobsdone -> ok
    end,
    timer:sleep(?TIME_BEFORE_DOING_MORE_JOBS),
    scheduler().

get_pid_scheduler() ->
    whereis(scheduler).