%forma de mandarle los datos
% [IP:<ip>:PUERTO:<puerto>,CPU:<Ncpu>,MEM,<Nmem>,(GPU:<Ngpu> opcional)]

-module(job_manager).

-export([]).

-define(PUERTOC, 3940).
-define(HOST, "localhost").
-define(CONFIGCONNECT, [{active,false},{packet,0}]).
-define(SEC, 1000).
-define(MAX_TIME_WORKING, 10 * SEC).
-define(MAX_TIME_ASKING_FOR_NODES, 50 *SEC).

-record(recursos,{cpu,mem,gpu}).
-record(direccion,{ip,puerto}).

%[{{192.168.1.10,8100},[{cpu:1},{mem:2},{gpu:3}]}]

manager_init() ->
    SockC = connect_to_local_node(),
    ManagerPid = spawn(?MODULE,manager,[SockC]),
    register(manager,Pid),
    FormatedNodesPid = spawn(?MODULE,get_formated_nodes,[SockC]).
    register(get_nodes,FormatedNodesPid),

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

        _ -> io:fwrite("sos un hijo de puta")
    end;

format_nodes([Node | Nodes]) ->
    Map1 = format_nodes(Node),
    Map2 = format_nodes(Nodes),
    maps:merge(Map1,Map2).

get_random_resources(Nodes) ->
    Dirs = maps:keys(Nodes),
    NodeSelected = maps:get(rand:uniform(length(Dirs)),Nodes),
    maps:find("",NodeSelected)

connect_to_local_node() ->
    case gen_tcp:connect(?HOST,?PUERTOC,?CONFIGCONNECT) of
        {ok,SockC} -> SockC;
        {error,Reason} -> io:fwrite("Error al conectarse al nodo, razon:~p ~n",[Reason]),exit(Reason)
    end,
    SockC.
%NODES 192.168.1.10:8100:cpu:4:mem:8192:gpu:1;192.168.1.11:8101:cpu:2:mem:4096
%NODES ["192.168.1.10:8100:cpu:4:mem:8192:gpu:1"],["192.168.1.11:8101:cpu:2:mem:4096"]

get_formated_nodes(SockC) -> 
    receive
        get ->
            ok = gen_tcp:send(SockC, "GET_NODES\n"),
            case gen_tcp:recv(Sock, 0) of
                {ok,Response} -> 
                    Nodes = string:lexemes(Response,";"),
                    ParsedNodes = format_nodes(Nodes),
                    manager ! {ok,ParsedNodes};

                {error,Reason} -> 
                    io:fwrite("Error al recibir la lista de nodos, razon:~p ~n",[Reason]),
                    manager ! {error, Reason}
            end
    end,
    get_formated_nodes(SockC).



manager() ->
    receive
        {ok,ParsedNodes} -> 
            Requests = get_random_resources(ParsedNodes),
                    
            %cuando se envian todos los trabajos y llegan sus repuestas
            get_nodes ! get;
        {error,Reason} ->
    end,
    manager().

generate_jobs(Requests,JobId) ->
    Job_REQUEST = "JOB_REQUEST " ++ JobId ++ " " ++ Requests ++ " \n",
    ok = gen_tcp:send(SockC, Job_REQUEST),
    case gen_tcp:recv(SockC, 0) of
        {ok, "JOB_GRANTED" ++ JobId} -> job_start();
        {ok, "JOB_DENIED" ++ JobId} -> mal;
        {ok, "JOB_TIMEOUT" ++ JobId} -> ?
    end.
    %JobId = erlang:unique_integer([positive]),

job_start() -> 
    Time = rand:uniform(?MAXTIMEJOB),
    time:sleep(Time*1000),
    JOB_RELEASE = "JOB_RELEASE " ++ JobId ++ " \n"
    ok = gen_tcp:send(SockC, JOB_RELEASE),
    jobDone.

%mandar
JOB_REQUEST = "JOB_REQUEST " ++ JobId ++ " \n" 
JOB_RELEASE = "JOB_RELEASE " ++ JobId ++ " \n"
JOB_STATUS = "JOB_STATUS " ++ JobId ++ (STATUS?) ++ " \n"
ok = gen_tcp:send(SockC, "JOB_REQUEST\n"),

%recivir
JOB_GRANTED = "JOB_GRANTED " ++ JobId ++ " \n" 
JOB_DENIED = "JOB_DENIED " ++ JobId ++ " \n"
JOB_TIMEOUT = "JOB_TIMEOUT " ++ JobId ++" \n"
ok = gen_tcp:recv(SockC, 0),



log({warn,Msg}) ->
    io:fwrite("Warning: ~p~n",[Msg]),
    file:write_file("logErl.txt",Msg,append).
log({fatal,Msg}) ->
    io:fwrite("Fatal: ~p~n",[Msg]),
    file:write_file("logErl.txt",Msg,append).
    exit(Msg).