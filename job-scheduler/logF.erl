-module(logF).
-export([cerrar/0,cerrar_todo/0,log/1,log/2]).
%hacer logserver

cerrar() ->
    receive
        close -> 
            send_recv_manager:cerrar_send_manager()
    end.

cerrar_todo() ->
    cerrar_todo ! close.

log(Msg) ->
    ok = io:fwrite(Msg),
    file:write_file("logErl.txt",[Msg ++ "\n"],[append]).

log(Msg,Args) ->
    ok = io:fwrite(Msg,Args),
    Str = io_lib:format(Msg,Args),
    file:write_file("logErl.txt",[string:chomp(Str) ++ "\n"],[append]).