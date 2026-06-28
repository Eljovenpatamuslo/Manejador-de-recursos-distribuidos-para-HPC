-module(manejador_recursos).


-export([obtener_recursos_para_jobs/1,format_nodes/1,obtener_y_formatear_nodos/0]).

-record(recursos,{cpu,mem,gpu}).
-record(direccion,{ip,puerto}).

%obtiene los nodos del agente c y lo formate de la forma [{recursos,direccion},...]
obtener_y_formatear_nodos() ->
    case send_recv_manager:obtener_nodos() of
        {ok,Response} ->
            logF:log(msg,"Nodos ~p ~n",[Response]), 
            Nodes = string:lexemes(Response,";"),
            ParsedNodes = format_nodes(Nodes),
            ParsedNodes;

        {error,Razon} -> 
            logF:log(msg,"Error al recibir la lista de nodos, razon:~p ~n",[Razon]),
            {error, Razon}
    end.

%obtiene los nodos y los separa por : y guarda cada dato
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

        _ -> logF:log(msg,"Error: el orden de la solicitud de recursos no es compatible")
        
    end;

format_nodes([Node | Nodes]) ->
    List1 = format_nodes([Node]),
    List2 = format_nodes(Nodes),
    lists:append(List1,List2).


%% Recibe directamente la lista de tuplas [{#direccion{}, #recursos{}}, ...]
obtener_recursos_para_jobs(ParsedNodes) ->
    Nod = [f(X) || X <- ParsedNodes],
    %logF:log(msg,"f(ParsedNodes):~p ~n",[Nod]),
    %%filtramos listas vacías
    lists:filter(fun(E) -> E /= " " end, h(Nod)).

%% caso base
f({_Dir, #recursos{cpu=0, mem=0, gpu=0}}) ->
    [];

%% caso recursivo
f({#direccion{ip=Ip, puerto=_} = Dir, #recursos{cpu=CpuR, mem=MemR, gpu=GpuR}}) ->
    
    CantCpu = if
        CpuR > 0 -> rand:uniform(CpuR); 
        true -> 0 
    end,
    CantMem = if 
        MemR == 0 -> 0;
        MemR >= 1024 -> 
            GbMem = floor(MemR / 1024),
            1024 * rand:uniform(GbMem); 
        true -> 
            MemR
    end,
    CantGpu = if 
        GpuR > 0 -> rand:uniform(GpuR); 
        true -> 0 
    end,
    
    StrCpu = if 
        CantCpu > 0 -> "@" ++ Ip ++ ":cpu:" ++ integer_to_list(CantCpu); 
        true -> "" 
    end,

    StrMem = if
        CantMem > 0 -> 
            Before = if
                StrCpu /= "" -> " ";
                true -> ""
            end,
            Before ++ "@" ++ Ip ++ ":mem:" ++ integer_to_list(CantMem);
            
        true -> "" 
    end,
    StrGpu = if 
        CantGpu > 0 -> 
            Before1 = if
                StrCpu /= "" -> " ";
                StrMem /= "" -> " ";
                true -> ""
            end,
            Before1 ++ "@" ++ Ip ++ ":gpu:" ++ integer_to_list(CantGpu);
        
        true -> "" 
    end,
    
    NodeStr = StrCpu ++ StrMem ++ StrGpu,
    Restantes = #recursos{cpu=CpuR-CantCpu, mem=MemR-CantMem, gpu=GpuR-CantGpu},
    Rest = f({Dir, Restantes}),
    
    if
        Rest == [] -> [NodeStr | Rest];
        true -> [NodeStr ++ "" | Rest]
    end.


h([]) ->
    [];
h(V) ->
    {G, V1} = g(V, []),
    case G of
        [] -> h(V1);
        _  -> [G | h(V1)]
    end.

g([], V) ->
    {[], V};
g([Node | Nodes], V1) ->
    case Node of
        [] ->
            g(Nodes, V1);
        _ ->
            Recurso = lists:nth(rand:uniform(length(Node)), Node),
            NewNode = lists:delete(Recurso, Node),
            
            {N, V} = g(Nodes, [NewNode | V1]),
            
            case N of
                %%parseamos y ordenamos los strings
                [] -> {Recurso, V};
                _  -> 
                    TokensIdénticos = string:lexemes(Recurso ++ " " ++ N, " "),
                    TokensOrdenados = lists:sort(TokensIdénticos),
                    StringSeguro = string:join(TokensOrdenados, " "),
                    {StringSeguro, V} 
            end
    end.