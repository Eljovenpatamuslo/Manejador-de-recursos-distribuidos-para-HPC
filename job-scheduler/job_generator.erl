-module(job_generator).
-compile(export_all).

-record(recursos,{cpu,mem,gpu}).
-record(direccion,{ip,puerto}).

%% Recibe directamente la lista de tuplas [{#direccion{}, #recursos{}}, ...]
obtener_recursos_para_jobs(ParsedNodes) ->
    Nod = [f(X) || X <- ParsedNodes],
    %%filtramos listas vacías
    lists:filter(fun(E) -> E /= [] end, h(Nod)).

%% caso base
f({_Dir, #recursos{cpu=0, mem=0, gpu=0}}) ->
    [];

%% caso recursivo
f({#direccion{ip=Ip, puerto=Puerto} = Dir, #recursos{cpu=CpuR, mem=MemR, gpu=GpuR}}) ->
    
    CantCpu = if CpuR > 0 -> rand:uniform(CpuR); true -> 0 end,
    CantMem = if MemR > 0 -> rand:uniform(MemR); true -> 0 end,
    CantGpu = if GpuR > 0 -> rand:uniform(GpuR); true -> 0 end,
    
    case {CantCpu, CantMem, CantGpu} of
        {0, 0, 0} -> 
            %% Si dio {0,0,0} forzamos otro intento
            f({Dir, #recursos{cpu=CpuR, mem=MemR, gpu=GpuR}});
        _ ->

            StrCpu = if CantCpu > 0 -> ":cpu:" ++ integer_to_list(CantCpu); true -> "" end,
            StrMem = if CantMem > 0 -> ":mem:" ++ integer_to_list(CantMem); true -> "" end,
            StrGpu = if CantGpu > 0 -> ":gpu:" ++ integer_to_list(CantGpu); true -> "" end,
            
            NodeStr = "@" ++ Ip ++ ":" ++ Puerto ++ StrCpu ++ StrMem ++ StrGpu,
    
            Restantes = #recursos{cpu=CpuR-CantCpu, mem=MemR-CantMem, gpu=GpuR-CantGpu},
            [NodeStr | f({Dir, Restantes})]
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