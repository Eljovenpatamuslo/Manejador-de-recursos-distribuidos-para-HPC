%forma de mandarle los datos
% [IP:<ip>:PUERTO:<puerto>,CPU:<Ncpu>,MEM,<Nmem>,(GPU:<Ngpu> opcional)]

%NODES 192.168.1.10:8100:cpu:4:mem:8192:gpu:1;192.168.1.11:8101:cpu:2:mem:4096

-module(send_recv_manager).

-export([send_recv_init/1,conectarse_a_nodo_local/2,esperar_respuesta_nodo/1,
    send_manager/1,obtener_nodos/0,enviar_send/1]).

%usar puerto 12000
-define(HOST, "localhost").
-define(CONFIGCONNECT, [{active,false},{packet,0}]).
-define(INTENTOS, 10).
-define(SEC, 1000).
-define(TIMEOUT, 60 * ?SEC).

send_recv_init(PuertoC) ->
    {ok, SockC} = conectarse_a_nodo_local(PuertoC,?INTENTOS),
    register(sockC,SockC),

    SendPid = spawn_link(?MODULE,send_manager,[SockC]),
    register(send_manager,SendPid),

    RecvPid = spawn_link(?MODULE,esperar_respuesta_nodo,[SockC]),
    register(esperar_respuesta_nodo,RecvPid),

    ok.

conectarse_a_nodo_local(_,0) ->
    logF:log(fatal,"Se acabaron los intentos, no se pudo establecer coneccion con nodo local~n");
conectarse_a_nodo_local(PuertoC,Intentos) ->
    case gen_tcp:connect(?HOST,PuertoC,?CONFIGCONNECT) of
        {ok,SockC} -> 
            logF:log(msg,"Erlang se conecto al nodo local~n"),
            {ok,SockC};
        {error,Razon} -> 
            logF:log(msg,"Error al conectarse al nodo, razon:~p , intentando otra vez...~n",[Razon]),
            conectarse_a_nodo_local(PuertoC,Intentos-1)
    end.

esperar_respuesta_nodo(SockC) ->
    case gen_tcp:recv(SockC, 0,?TIMEOUT) of     
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
            logF:log(fatal,"Error al recibir mensaje del nodo, razon:~p ~n",[Razon]);

        M -> 
            logF:log(msg,"Error, mensaje: ~p inesperado ~n",[M]),
            esperar_respuesta_nodo(SockC)
    end.

send_manager(SockC) ->
    receive
        {jobRelease,JobId} -> 
            JOB_RELEASE = "JOB_RELEASE " ++ integer_to_list(JobId) ++ "\n",
            case gen_tcp:send(SockC, JOB_RELEASE) of
                ok -> 
                    ok;
                {error,Razon} -> 
                    logF:log(msg,"Error al enviar JOB_RELEASE, razon: ~p ~n",[Razon]),
                    send_manager(SockC)
            end;

        {jobRequest, JobId, Recursos} ->
            JOB_REQUEST = "JOB_REQUEST " ++ integer_to_list(JobId) ++ " " ++ Recursos ++ "\n",
            case gen_tcp:send(SockC, JOB_REQUEST) of
                ok -> ok;
                {error,Razon} -> 
                    logF:log(msg,"Error al enviar JOB_REQUEST, razon: ~p ~n",[Razon]),
                    send_manager(SockC)
            end;
        getNodes -> 
            case gen_tcp:send(SockC, "GET_NODES\n") of
                ok -> ok;
                {error,Razon} -> 
                    logF:log(msg,"Error al enviar GET_NODES, razon: ~p ~n",[Razon]),
                    send_manager(SockC)
            end;
        M ->
            logF:log(msg,"Error, mensaje: ~p inesperado ~n",[M]),
            send_manager(SockC)
    end,
    send_manager(SockC).

obtener_nodos() ->
    send_manager ! getNodes,
    receive
        {ok,Nodos} -> {ok,Nodos};
        {error,Razon} -> 
            logF:log(msg,"No se pudo obtener los nodos, razon:~p ~n",[Razon]),
            {error,Razon};
        M ->
            logF:log(msg,"Error, mensaje: ~p inesperado ~n",[M]),
            logF:cerrar_todo()
    end.

enviar_send(Msg) ->
    send_manager ! Msg.