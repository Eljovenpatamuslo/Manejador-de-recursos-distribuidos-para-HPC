%forma de mandarle los datos
% [IP:<ip>:PUERTO:<puerto>,CPU:<Ncpu>,MEM,<Nmem>,(GPU:<Ngpu> opcional)]

%NODES 192.168.1.10:8100:cpu:4:mem:8192:gpu:1;192.168.1.11:8101:cpu:2:mem:4096


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
    Nod = [asignar_recursos_random(X) || X <- ParsedNodes],
    %%filtramos listas vacías
    lists:filter(fun(E) -> E /= " " end, repartir_recursos_para_trabajos(Nod)).

%% caso base
asignar_recursos_random({_Dir, #recursos{cpu=0, mem=0, gpu=0}}) ->
    [];

%% caso recursivo
asignar_recursos_random({#direccion{ip=Ip, puerto=_} = Dir, #recursos{cpu=CpuR, mem=MemR, gpu=GpuR}}) ->
    
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
    Rest = asignar_recursos_random({Dir, Restantes}),
    
    if
        Rest == [] -> [NodeStr | Rest];
        true -> [NodeStr ++ "" | Rest]
    end.


repartir_recursos_para_trabajos([]) ->
    [];
repartir_recursos_para_trabajos(V) ->
    {G, V1} = repartir_recursos_para_trabajos_aux(V, []),
    case G of
        [] -> repartir_recursos_para_trabajos(V1);
        _  -> [G | repartir_recursos_para_trabajos(V1)]
    end.

repartir_recursos_para_trabajos_aux([], V) ->
    {[], V};
repartir_recursos_para_trabajos_aux([Node | Nodes], V1) ->
    case Node of
        [] ->
            repartir_recursos_para_trabajos_aux(Nodes, V1);
        _ ->
            Recurso = lists:nth(rand:uniform(length(Node)), Node),
            NewNode = lists:delete(Recurso, Node),
            
            {N, V} = repartir_recursos_para_trabajos_aux(Nodes, [NewNode | V1]),
            
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