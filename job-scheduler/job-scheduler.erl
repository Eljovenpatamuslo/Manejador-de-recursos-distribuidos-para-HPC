-module(job_manager).
-export([manager_init/0,job_manager/2,controlador_mensajes/3]).

-export([connect_to_local_node/0]).

-define(PORTC, 3940).
-define(HOST, "localhost").
-define(CONFIGCONNECT, [{active,false},{packet,0}]).

connect_to_local_node() ->
    case gen_tcp:connect(?HOST,?PORTC,?CONFIGCONNECT) of
        {ok,Sock_C} -> Sock_C;
        {error,Reason} -> io:fwrite("Error al conectarse al nodo, razon:~p ~n",[Reason]),exit(Reason)
    end,
    Sock_C.

manager_init() ->
    Sock_C = connect_to_local_node(),
    job_manager([],Sock_C).

job_manager() ->
    spawn(?MODULE,job_init,[]),
    todo.

job_init() ->
    JobId = erlang:unique_integer([positive]),
    end.

controlador_mensajes(Sock,IdJob,Sock_C) ->
    case gen_tcp:recv(Sock, 0) of
        {ok, "GET_NODES\n"} ->
            ok = gen_tcp:send(Sock_C, "GET_NODES\n"),
            {ok, Response} = gen_tcp:recv(Sock_C, 0),
            io:fwrite("respuesta : ~p ~n",[Response]),
            controlador_mensajes(Sock,IdJob,Sock_C);

        {ok, Msg} ->
            io:fwrite("trabajo ~p: ~p ~n",[IdJob,Msg]),
            ok = gen_tcp:send(Sock_C, Msg),
            {ok, Response} = gen_tcp:recv(Sock_C, 0),
            io:fwrite("respuesta : ~p ~n",[Response]),
            controlador_mensajes(Sock,IdJob,Sock_C);

        {error, closed} ->
            ok = gen_tcp:close(Sock),
            io:fwrite("server cerrado ~n"),
            closed
    end.


