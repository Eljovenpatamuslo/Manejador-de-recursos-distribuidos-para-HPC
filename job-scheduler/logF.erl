-module(logF).
-export([log/2,log/3,crear_error_managment/0,logServer/0]).

-define(SEC, 1000).
-define(TIEMPO_ANTES_DE_TERMINAR, 5 * ?SEC).

crear_error_managment() ->
    Pid = spawn_link(?MODULE,logServer,[]),
    register(log,Pid),

    log(msg,"---Log del scheduler de erlang---~n"),
    log(msg,"fecha: [~p] hora: [~p] ~n",[date(),time()]),
    ok.

logServer() ->
    receive
        {msg,Msg,Args} ->
            ok = io:fwrite(Msg,Args),
            Str = io_lib:format(Msg,Args),
            file:write_file("logErl.txt",[string:chomp(Str) ++ "\n"],[append]),
            logServer();
        {fatal,Msg,Args} ->
            ok = io:fwrite(Msg,Args),
            Str = io_lib:format(Msg,Args),
            file:write_file("logErl.txt",[string:chomp(Str) ++ "\n"],[append]),
            file:write_file("logErl.txt","---TERMINADO---\n",[append]),
            timer:sleep(?TIEMPO_ANTES_DE_TERMINAR),
            exit(fatal)
    end.

log(Type,Msg) ->
    case whereis(log) of
        undefined -> 
            io:fwrite("Log del scheduler de erlang cerrado~n"),
            exit(fatal);
        LogPid -> LogPid ! {Type,Msg,[]}
    end.

log(Type,Msg,Args) ->
    case whereis(log) of
        undefined -> io:fwrite("Log del scheduler de erlang cerrado~n"),
        exit(fatal);
        LogPid -> LogPid ! {Type,Msg,Args}
    end.
    
