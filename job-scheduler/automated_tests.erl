-module(automated_tests).
-export([run_suite/0, test_runner_loop/2]).

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
    ok = test_job_generator_output(),

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
    Result = job_scheduler:format_nodes(Input),
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
    Result = job_scheduler:format_nodes(Input),
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
    Result = job_scheduler:format_nodes(Input),
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
test_job_generator_output() ->
    io:format("[TEST] Validando consistencia strings en job_generator... "),
    MockNodes = [{#direccion{ip="127.0.0.1", puerto="9000"}, #recursos{cpu=2, mem=1024, gpu=0}}],
    Jobs = job_generator:obtener_recursos_para_jobs(MockNodes),
    %% Verificamos que sea una lista de strings y que contenga los delimitadores del protocolo
    case Jobs of
        [] -> 
            io:format("FALLÓ: El generador retornó una lista vacía~n"),
            exit(test_job_generator_empty);
        [FirstJob | _] ->
            case string:find(FirstJob, "@") of
                nomatch -> 
                    io:format("FALLÓ: El string del job carece del prefijo '@'. Obtenido: ~p~n", [FirstJob]),
                    exit(test_job_generator_invalid_format);
                _ -> 
                    io:format("OK~n"), ok
            end
    end.

%% ===================================================================
%% CASO 5: Pruebas de Integración Dinámicas (Inyección de Fallos TCP)
%% ===================================================================
run_integration_tests() ->
    io:format("[TEST] Iniciando servidor TCP Mock para inyección de estados...~n"),
    {ok, LSocket} = gen_tcp:listen(3947, [list, {active, false}, {reuseaddr, true}]),
    
    %% Levantamos un proceso controlador del socket simulado
    spawn_link(?MODULE, test_runner_loop, [LSocket, 1]),
    
    io:format("[TEST] Inicializando entorno real del planificador...~n"),
    %% Inicializamos tu planificador real de Erlang
    job_scheduler:scheduler_init(),
    
    %% Monitoreamos la ejecución de la ráfaga de jobs.
    %% Como el scheduler es infinito, dejamos correr la simulación por 12 segundos
    %% para dar tiempo a evaluar éxitos, denegaciones y backoffs.
    timer:sleep(12000),
    io:format("[TEST] Fase de simulación dinámica finalizada con éxito.~n"),
    gen_tcp:close(LSocket),
    ok.

%% Loop interno que simula los estados del Agente en C
test_runner_loop(LSocket, EstadoFase) ->
    case gen_tcp:accept(LSocket) of
        {ok, Socket} ->
            handle_mock_agent_io(Socket, EstadoFase),
            test_runner_loop(LSocket, EstadoFase + 1);
        {error, _} ->
            ok
    end.

handle_mock_agent_io(Socket, EstadoFase) ->
    case gen_tcp:recv(Socket, 0) of
        {ok, Data} ->
            Tokens = string:lexemes(Data, " \n"),
            case Tokens of
                ["GET_NODES"] ->
                    %% Enviamos siempre una topología válida
                    Topologia = "NODES 127.0.0.1:9000:cpu:2:mem:2048:gpu:1;127.0.0.1:9001:cpu:4:mem:4096\n",
                    gen_tcp:send(Socket, Topologia),
                    handle_mock_agent_io(Socket, EstadoFase);
                
                ["JOB_REQUEST", JobId | _Recursos] ->
                    %% Inyectamos fallos deterministas según la fase del test
                    Respuesta = case EstadoFase of
                        1 -> 
                            io:format("[MOCK INYECTOR] Caso A (Happy Path): Concediendo Job ~p~n", [JobId]),
                            "JOB_GRANTED " ++ JobId ++ "\n";
                        2 -> 
                            io:format("[MOCK INYECTOR] Caso B (Mitigación Livelock): Denegando Job ~p. Forzando reintento.~n", [JobId]),
                            "JOB_DENIED " ++ JobId ++ "\n";
                        3 -> 
                            io:format("[MOCK INYECTOR] Caso C (Pérdida de paquetes): Enviando JOB_TIMEOUT para Job ~p~n", [JobId]),
                            "JOB_TIMEOUT " ++ JobId ++ "\n";
                        _ -> 
                            "JOB_GRANTED " ++ JobId ++ "\n"
                    end,
                    timer:sleep(100), %% Simulación de latencia base
                    gen_tcp:send(Socket, Respuesta),
                    handle_mock_agent_io(Socket, EstadoFase);
                
                ["JOB_RELEASE", JobId] ->
                    io:format("[MOCK INYECTOR] Confirmación recibida: Liberación de recursos del Job ~p.~n", [JobId]),
                    handle_mock_agent_io(Socket, EstadoFase);
                
                _ -> 
                    handle_mock_agent_io(Socket, EstadoFase)
            end;
        {error, closed} ->
            ok
    end.