-module(job_generator).
-compile(export_all).

-record(recursos,{cpu,mem,gpu}).
-record(direccion,{ip,puerto}).
% 192.168.1.10:8100:cpu:4:mem:8192:gpu:1;192.168.1.11:8101:cpu:2:mem:4096;192.168.1.11:8101:cpu:2:mem:4096
obtener_recursos_para_jobs(Nodes) ->
    Nodes1 = string:lexemes(Nodes,";"),
    ParsedNodes = job_scheduler:format_nodes(Nodes1),
    Nod = [f(X) || X <- ParsedNodes],
    lists:delete([],h(Nod)).

f({#direccion{ip=Ip, puerto=Puerto}, #recursos{cpu=0, mem=0, gpu=0}}) ->
    [];
f({#direccion{ip=Ip, puerto=Puerto}, #recursos{cpu=CpuRestante, mem=MemRestante, gpu=GpuRestante}}) ->
    CantCpuAgarrada = 
        case CpuRestante of 
            0 -> 0;
            _ -> rand:uniform(CpuRestante)
        end,
    
    ListCpuAgarrada = 
        case CantCpuAgarrada of 
            0 -> [];
            _ -> "cpu:" ++ integer_to_list(CantCpuAgarrada)
        end,

    CantMemAgarrada = 
        case MemRestante of 
            0 -> 0;
            _ -> rand:uniform(MemRestante)
        end,

    ListMemAgarrada = 
        case CantMemAgarrada of 
            0 -> [];
            _ -> "mem:" ++ integer_to_list(CantMemAgarrada)
        end,

    CantGpuAgarrada = 
        case GpuRestante of 
            0 -> 0;
            _ -> rand:uniform(GpuRestante)
        end,

    ListGpuAgarrada = 
        case CantGpuAgarrada of 
            0 -> [];
            _ -> "gpu:" ++ integer_to_list(CantGpuAgarrada)
        end,
    
    f({#direccion{ip = Ip, puerto = Puerto}, #recursos{cpu = CpuRestante-CantCpuAgarrada, mem = MemRestante-CantMemAgarrada, gpu = GpuRestante-CantGpuAgarrada}})
    ++ 
    ["@"++ Ip ++ ":" ++ ListCpuAgarrada ++ ListMemAgarrada ++ ListGpuAgarrada].
h([]) ->
    [];
h(V) ->
    {G,V1} = g(V,[]),
    [G] ++ h(V1).
g([],V) ->
    {[],V};
g([Node | Nodes],V1) ->
    case Node of
        [] ->
            g(Nodes,V1);
        _ ->
            Recurso = lists:nth(rand:uniform(length(Node)),Node),
            NewNode = lists:delete(Recurso,Node),
            {N,V} = g(Nodes,[NewNode]++V1),
            case N of
                [] -> {Recurso,V};
                _ -> {Recurso ++ " " ++ N,V} 
            end
    end.


