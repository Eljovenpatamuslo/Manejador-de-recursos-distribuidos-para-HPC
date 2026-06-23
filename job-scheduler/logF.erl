-module(logF).
-export([log/2,log/3,crear_error_managment/0,logServer/0]).
%hacer logserver

crear_error_managment() ->
    Pid = spawn(?MODULE,logServer,[]),
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
            log(msg,"---Terminando---~n"),
            timer:sleep(1000),
            exit(fatal)
    end.

log(Type,Msg) ->
    log ! {Type,Msg,[]}.
    

log(Type,Msg,Args) ->
    log ! {Type,Msg,Args}.
