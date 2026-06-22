-module(logF).
-export([log/2,log/3,crear_error_managment/0,logServer/0]).
%hacer logserver

crear_error_managment() ->
    Pid = spawn_link(?MODULE,logServer,[]),
    register(log,Pid),

    log(msg,"---Log del scheduler de erlang---~n"),
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
            exit(fatal)
    end.

log(Type,Msg) ->
    log ! {Type,Msg,[]}.
    

log(Type,Msg,Args) ->
    log ! {Type,Msg,Args}.
