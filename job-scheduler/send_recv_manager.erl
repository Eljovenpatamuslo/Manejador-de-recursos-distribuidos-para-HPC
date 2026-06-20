%forma de mandarle los datos
% [IP:<ip>:PUERTO:<puerto>,CPU:<Ncpu>,MEM,<Nmem>,(GPU:<Ngpu> opcional)]

%NODES 192.168.1.10:8100:cpu:4:mem:8192:gpu:1;192.168.1.11:8101:cpu:2:mem:4096

-module(send_recv_manager).

-export([send_recv_init/0,conectarse_a_nodo_local/0,esperar_respuesta_nodo/1,
    send_manager/1,obtener_nodos/0,enviar_send/1,cerrar_send_manager/0]).

-define(PUERTOC, 3947). %usar 12000
-define(HOST, "localhost").
-define(CONFIGCONNECT, [{active,false},{packet,0}]).

send_recv_init() ->
    {ok, SockC} = conectarse_a_nodo_local(),
    SendPid = spawn(?MODULE,send_manager,[SockC]),
    register(send_manager,SendPid),
    RecvPid = spawn(?MODULE,esperar_respuesta_nodo,[SockC]),
    register(esperar_respuesta_nodo,RecvPid).

conectarse_a_nodo_local() ->
    case gen_tcp:connect(?HOST,?PUERTOC,?CONFIGCONNECT) of
        {ok,SockC} -> 
            logF:log("Erlang se conecto al nodo local"),
            {ok,SockC};
        {error,Razon} -> 
            logF:log("Error al conectarse al nodo, razon:~p ~n",[Razon]),
            logF:cerrar_todo()
    end.

esperar_respuesta_nodo(SockC) ->
    case gen_tcp:recv(SockC, 0) of     
        {ok, "JOB_GRANTED "++ JobIdn} -> 
            JobId = list_to_integer(string:reverse(string:prefix(string:reverse(JobIdn),"\n"))),
            job_server:enviar_estado_job(jobGranted,JobId),

            esperar_respuesta_nodo(SockC);
        {ok, "JOB_DENIED "++ JobIdn} -> 
            JobId = list_to_integer(string:reverse(string:prefix(string:reverse(JobIdn),"\n"))),
            job_server:enviar_estado_job(jobDenied,JobId),

            esperar_respuesta_nodo(SockC);
        {ok, "JOB_TIMEOUT "++ JobIdn} -> 
            JobId = list_to_integer(string:reverse(string:prefix(string:reverse(JobIdn),"\n"))),
            job_server:enviar_estado_job(jobTimeout,JobId),

            esperar_respuesta_nodo(SockC);
        {ok,"NODES " ++ Nodesn} -> 
            Nodes = string:reverse(string:prefix(string:reverse(Nodesn),"\n")),
            Scheduler = job_scheduler:get_pid_scheduler(),
            Scheduler ! {ok, Nodes},

            esperar_respuesta_nodo(SockC);
        {error,Razon} -> 
            logF:log("Error al recibir mensaje del nodo, razon:~p ~n",[Razon]),
            logF:cerrar_todo();

        M -> 
            logF:log("Error, mensaje: ~p inesperado ~n",[M]),
            logF:cerrar_todo()
    end.

send_manager(SockC) ->
    receive
        {jobRelease,JobId} -> 
            JOB_RELEASE = "JOB_RELEASE " ++ integer_to_list(JobId) ++ "\n",
            case gen_tcp:send(SockC, JOB_RELEASE) of
                ok -> 
                    ok;
                {error,Razon} -> 
                    logF:log("Error al enviar JOB_RELEASE, razon: ~p ~n",[Razon])
            end;

        {jobRequest, JobId, Recursos} ->
            JOB_REQUEST = "JOB_REQUEST " ++ integer_to_list(JobId) ++ " " ++ Recursos ++ "\n",
            case gen_tcp:send(SockC, JOB_REQUEST) of
                ok -> ok;
                {error,Razon} -> 
                    logF:log("Error al enviar JOB_REQUEST, razon: ~p ~n",[Razon])
            end;
        getNodes -> 
            case gen_tcp:send(SockC, "GET_NODES\n") of
                ok -> ok;
                {error,Razon} -> 
                    logF:log("Error al enviar GET_NODES, razon: ~p ~n",[Razon])
            end;
        close ->
            logF:log("Cerrando: ~p ~n",[?FUNCTION_NAME]),
            logF:cerrar_todo(); %haceralgo
        M ->
            logF:log("Error, mensaje: ~p inesperado ~n",[M]),
            logF:cerrar_todo()
    end,
    send_manager(SockC).

obtener_nodos() ->
    send_manager ! getNodes,
    receive
        {ok,Nodos} -> {ok,Nodos};
        {error,Razon} -> 
            logF:log("No se pudo obtener los nodos, razon:~p ~n",[Razon]),
            {error,Razon};
        M ->
            logF:log("Error, mensaje: ~p inesperado ~n",[M]),
            logF:cerrar_todo()
    end.

enviar_send(Msg) ->
    send_manager ! Msg.

cerrar_send_manager() ->
    send_manager ! close.
