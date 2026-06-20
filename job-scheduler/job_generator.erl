-module(job_generator).
-compile(export_all).

obtener_recursos_para_jobs(Nodes) ->
    todo,
    ["@192.168.1.2:cpu:2 @192.168.1.3:gpu:1","@192.168.1.2:cpu:2 @192.168.1.3:gpu:1"].
    %Dirs = maps:keys(Nodes),
    %NodeSelected = maps:get(rand:uniform(length(Dirs)),Nodes),
    %maps:find("",NodeSelected)


crear_peticion(NodosDisponibles, Requisitos) ->
    %% Iniciamos semilla para que la selección sea verdaderamente aleatoria por Job
    rand:seed(exsp),
    asignar_recursos(NodosDisponibles, Requisitos, []).

%% Caso Base: Ya asignamos todos los requisitos
asignar_recursos(_Nodos, [], Acumulador) ->
    Acumulador;

%% Paso Recursivo: Tomamos un requisito (ej. {cpu, 2}) y elegimos un nodo al azar
asignar_recursos(Nodos, [{TipoRecurso, Cantidad} | RestoReqs], Acumulador) ->
    %% Mezclamos la lista de nodos para garantizar distribución uniforme (O(N))
    %NodosMezclados = mezclar_lista(Nodos),
    
    %% Elegimos el primer nodo de la lista mezclada que tenga la capacidad suficiente
    NodoElegido = buscar_nodo_capaz(Nodos, TipoRecurso, Cantidad),
    
    case NodoElegido of
        {error, no_capacity} ->
            io:fwrite("ADVERTENCIA: No hay ningún nodo en el clúster capaz de proveer ~p de ~p.~n", 
                      [Cantidad, TipoRecurso]),
            %% Abortamos la creación de este Job o lo manejamos según convenga
            exit(insufficient_cluster_capacity);
            
        {IpPuerto, _} ->
            %% Construimos la tupla en el formato que espera tu job_sch.erl
            NuevaPeticion = {IpPuerto, TipoRecurso, Cantidad},
            asignar_recursos(Nodos, RestoReqs, [NuevaPeticion | Acumulador])
    end.

%% @doc Busca linealmente un nodo que satisfaga el recurso pedido. Complejidad temporal: O(N)
buscar_nodo_capaz([], _Tipo, _Cantidad) ->
    {error, no_capacity};
buscar_nodo_capaz([Nodo | Resto], Tipo, Cantidad) ->
    {{IP, Puerto}, CpuMax, MemMax, GpuMax} = Nodo,
    
    Cumple = case Tipo of
        cpu -> CpuMax >= Cantidad;
        mem -> MemMax >= Cantidad;
        gpu -> GpuMax >= Cantidad
    end,
    
    if
        Cumple -> { {IP, Puerto}, Nodo };
        true -> buscar_nodo_capaz(Resto, Tipo, Cantidad)
    end.

%% @doc Implementación clásica del algoritmo de Fisher-Yates funcional para mezclar listas
mezclar_lista(Lista) ->
    ListaConClaves = [{rand:uniform(), Elemento} || Elemento <- Lista],
    ListaOrdenada = lists:sort(ListaConClaves),
    [Elemento || {_, Elemento} <- ListaOrdenada].