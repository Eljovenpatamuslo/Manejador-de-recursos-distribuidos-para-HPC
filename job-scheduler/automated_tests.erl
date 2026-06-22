-module(automated_tests).
-export([run_suite/0, test_runner_acceptor/1]).

-record(recursos,{cpu,mem,gpu}).
-record(direccion,{ip,puerto}).

%% ===================================================================
%% Ejecutor Principal de la Suite de Pruebas
%% ===================================================================
run_suite() ->
    io:format("~n==================================================~n"),
    io:format("INICIANDO SUITE DE TESTEO INTEGRAL PARA ERLANG CLUSTER~n"),
    io:format("==================================================~n"),
    
    %% 1. Pruebas Unitarias de Parsing y Formateo
    ok = test_parsing_con_gpu(),
    ok = test_parsing_sin_gpu(),
    ok = test_parsing_multiples_nodos(),
    
    %% 2. Pruebas Unitarias del Generador de Jobs
    ok = test_manejador_recursos_output(),

    %% 3. Pruebas de Integración con Inyección de Escenarios de Red
    ok = run_integration_tests(),
    
    io:format("~n==================================================~n"),
    io:format("¡TODOS LOS CASOS DE PRUEBA PASARON EXITOSAMENTE!~n"),
    io:format("==================================================~n~n").

%% ===================================================================
%% CASO 1: Parsing de Nodo Completo con GPU
%% ===================================================================
test_parsing_con_gpu() ->
    io:format("[TEST] Validando format_nodes/1 con especificación GPU... "),
    Input = ["192.168.1.10:8100:cpu:4:mem:8192:gpu:1"],
    Result = manejador_recursos:format_nodes(Input),
    case Result of
        [{#direccion{ip="192.168.1.10", puerto="8100"}, #recursos{cpu=4, mem=8192, gpu=1}}] ->
            io:format("OK~n"), ok;
        _ ->
            io:format("FALLÓ: Formato con GPU incorrecto. Obtenido: ~p~n", [Result]),
            exit(test_parsing_con_gpu_failed)
    end.

%% ===================================================================
%% CASO 2: Parsing de Nodo Estándar sin GPU
%% ===================================================================
test_parsing_sin_gpu() ->
    io:format("[TEST] Validando format_nodes/1 sin especificación GPU... "),
    Input = ["192.168.1.11:8101:cpu:2:mem:4096"],
    Result = manejador_recursos:format_nodes(Input),
    case Result of
        [{#direccion{ip="192.168.1.11", puerto="8101"}, #recursos{cpu=2, mem=4096, gpu=0}}] ->
            io:format("OK~n"), ok;
        _ ->
            io:format("FALLÓ: Formato sin GPU incorrecto. Obtenido: ~p~n", [Result]),
            exit(test_parsing_sin_gpu_failed)
    end.

%% ===================================================================
%% CASO 3: Parsing Recursivo de Múltiples Nodos en Red
%% ===================================================================
test_parsing_multiples_nodos() ->
    io:format("[TEST] Validando recursión de format_nodes/1 con múltiples nodos... "),
    Input = ["192.168.1.10:8100:cpu:4:mem:8192:gpu:1", "192.168.1.11:8101:cpu:2:mem:4096"],
    Result = manejador_recursos:format_nodes(Input),
    case length(Result) of
        2 ->
            io:format("OK~n"), ok;
        _ ->
            io:format("FALLÓ: No se parsearon todos los nodos. Lista: ~p~n", [Result]),
            exit(test_parsing_multiples_nodos_failed)
    end.

%% ===================================================================
%% CASO 4: Validación de Estructura del Generador Estocástico
%% ===================================================================
test_manejador_recursos_output() ->
    io:format("[TEST] Validando formato estricto @ip:puerto:recurso:cantidad... "),
    MockNodes = [{#direccion{ip="127.0.0.1", puerto="9000"}, #recursos{cpu=2, mem=1024, gpu=0}}],
    Jobs = manejador_recursos:obtener_recursos_para_jobs(MockNodes),
    
    case Jobs of
        [] -> 
            io:format("FALLÓ: El generador retornó una lista vacía~n"),
            exit(test_manejador_recursos_empty);
        [FirstJob | _] ->
            %% FirstJob tiene múltiples recursos separados por espacio. Agarramos el primero.
            [PrimerRecurso | _] = string:lexemes(FirstJob, " "),
            
            %% Partimos el recurso por los dos puntos ":"
            %% Debe resultar en exactamente 4 partes para ser válido.
            TokensRecurso = string:lexemes(PrimerRecurso, ":"),
            
            case TokensRecurso of
                ["@" ++ _Ip, _Puerto, TipoRecurso, _Cantidad] when 
                        TipoRecurso == "cpu"; TipoRecurso == "mem"; TipoRecurso == "gpu" -> 
                    io:format("OK~n"), ok;
                _ -> 
                    io:format("FALLÓ: El string no respeta la estructura estricta. Obtenido: ~p~n", [PrimerRecurso]),
                    exit(test_manejador_recursos_invalid_format)
            end
    end.

%% ===================================================================
%% CASO 5: Pruebas de Integración Dinámicas (Inyección de Fallos TCP)
%% ===================================================================
run_integration_tests() ->
    PuertoMock = 3947,
    io:format("[TEST] Iniciando servidor TCP Mock para inyección de estados en puerto ~p...~n", [PuertoMock]),
    {ok, LSocket} = gen_tcp:listen(PuertoMock, [list, {active, false}, {reuseaddr, true}, {packet, 0}]),
    
    %% Levantamos un proceso controlador del socket simulado
    spawn_link(?MODULE, test_runner_acceptor, [LSocket]),
    
    io:format("[TEST] Inicializando entorno real del planificador en background...~n"),
    
    %% INYECTAMOS EL PUERTO Y LO CORREMOS EN BACKGROUND PARA NO BLOQUEAR EL TEST
    spawn(fun() -> job_scheduler:scheduler_init(PuertoMock) end),
    
    %% Monitoreamos la ejecución.
    timer:sleep(12000),
    
    io:format("~n[TEST] Fase de simulación dinámica finalizada con éxito.~n"),
    gen_tcp:close(LSocket),
    
    %% Limpieza post-test para no dejar el entorno colgado
    catch unregister(scheduler),
    ok.

%% Acepta la conexión inicial de tu manager TCP
test_runner_acceptor(LSocket) ->
    case gen_tcp:accept(LSocket) of
        {ok, Socket} ->
            %% Iniciamos la fase 1 en cuanto se conecta
            handle_mock_agent_io(Socket, 1),
            test_runner_acceptor(LSocket);
        {error, _} ->
            ok
    end.

%% Bucle principal de respuesta estocástica
handle_mock_agent_io(Socket, EstadoFase) ->
    case gen_tcp:recv(Socket, 0) of
        {ok, Data} ->
            Tokens = string:lexemes(Data, " \n"),
            
            %% Lógica rotativa de fallos (1->2->3->1)
            SiguienteFase = if EstadoFase >= 3 -> 1; true -> EstadoFase + 1 end,

            case Tokens of
                ["GET_NODES"] ->
                    Topologia = "NODES 127.0.0.1:9000:cpu:2:mem:2048:gpu:1;127.0.0.1:9001:cpu:4:mem:4096\n",
                    gen_tcp:send(Socket, Topologia),
                    %% No avanzamos de fase por pedir los nodos
                    handle_mock_agent_io(Socket, EstadoFase);
                
                ["JOB_REQUEST", JobId | _Recursos] ->
                    Respuesta = case EstadoFase of
                        1 -> 
                            io:format("[MOCK INYECTOR] Caso A: Concediendo Job ~p~n", [JobId]),
                            "JOB_GRANTED " ++ JobId ++ "\n";
                        2 -> 
                            io:format("[MOCK INYECTOR] Caso B: Denegando Job ~p. Forzando reintento.~n", [JobId]),
                            "JOB_DENIED " ++ JobId ++ "\n";
                        3 -> 
                            io:format("[MOCK INYECTOR] Caso C: Enviando JOB_TIMEOUT para Job ~p~n", [JobId]),
                            "JOB_TIMEOUT " ++ JobId ++ "\n";
                        _ -> 
                            "JOB_GRANTED " ++ JobId ++ "\n"
                    end,
                    timer:sleep(100), %% Latencia base de simulación
                    gen_tcp:send(Socket, Respuesta),
                    %% Acá avanzamos al siguiente caso para obligar al scheduler a lidiar con todo
                    handle_mock_agent_io(Socket, SiguienteFase);
                
                ["JOB_RELEASE", JobId] ->
                    io:format("[MOCK INYECTOR] Confirmación recibida: Liberación del Job ~p.~n", [JobId]),
                    handle_mock_agent_io(Socket, EstadoFase);
                
                _ -> 
                    handle_mock_agent_io(Socket, EstadoFase)
            end;
        {error, closed} ->
            io:format("[MOCK INYECTOR] El cliente cerró la conexión TCP.~n"),
            ok
    end.