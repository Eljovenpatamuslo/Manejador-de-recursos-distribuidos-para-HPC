-module(job_server).

-export([iniciar_job_server/0,job_server/1,crear_job/1,
    ejecutar_trabajo/1,adquirir_loop/3,manejar_fallo/3,
    aplicar_timeout/2,enviar_estado_job/2,agregar_job/2,remover_job/1]).

-define(SEC, 1000).
-define(TIMEOUT, 30 * ?SEC).
-define(INTENTOS_HASTA_DESCARTAR, 5).

%inicia todo lo necesario para el job_server
iniciar_job_server() ->
    ServerPid = spawn_link(?MODULE,job_server,[maps:new()]),
    register(job_server,ServerPid),
    ok.

%proceso encargado de guardar los jobids asociados a su id de proceso para poder enviarles los mensajes del agente de c
job_server(JobMap) ->
    receive
        {find,JobAction, JobId} -> 
            JobPid = maps:get(JobId,JobMap,-1),
            case JobPid of
                -1 -> logF:log(msg,"No existe job asociado a la JobId:~p ~n",[JobId]);

                JobPid -> JobPid ! JobAction
            end,
            job_server(JobMap);
        {add,JobPid,JobId} -> 
            NewMap = maps:put(JobId,JobPid,JobMap),
            job_server(NewMap);
        {remove,JobId} -> 
            NewMap = maps:remove(JobId,JobMap),
            case maps:size(NewMap) of
                0 -> 
                    Manager = job_scheduler:get_pid_scheduler(),
                    Manager ! alljobsdone;
                _ -> jobdone 
            end,    
            job_server(NewMap);
        _ -> logF:log(msg,"solicitud: [~p] inesperada ~n"),
            job_server(JobMap)
    end.

%crea un job con los recursos dados
crear_job(Resource) -> 
    JobId = erlang:unique_integer([positive]),
    JobPid = spawn(?MODULE,adquirir_loop,[JobId,Resource,0]),
    agregar_job(JobPid,JobId).

%espera un rato haciendo nada y libera el trabajo
ejecutar_trabajo(JobId) ->
    logF:log(msg,"[Job ~p] Ejecutando procesamiento en el cluster simulado...~n", [JobId]),
    timer:sleep(5 * ?SEC),
    
    logF:log(msg,"[Job ~p] Procesamiento completado. Liberando infraestructura.~n", [JobId]),
    send_recv_manager:enviar_send({jobRelease,JobId}),
    remover_job(JobId),
    logF:log(msg,"[Job ~p] Terminado correctamente.~n", [JobId]),
    ok.

%cada trabajo envia un job_request con sus recusos y se quedan esperando su respuesta
adquirir_loop(JobId, Recursos, Intentos) ->
    logF:log(msg,"[Job ~p] Solicitando los recursos: ~p~n", [JobId, Recursos]),
    
    send_recv_manager:enviar_send({jobRequest, JobId, Recursos}),
    
    receive
        jobGranted -> 
            logF:log(msg,"[Job ~p] EXITO! Todos los recursos concedidos.~n", [JobId]),
            ejecutar_trabajo(JobId);
            
        jobDenied ->
            logF:log(msg,"[Job ~p] Trabajo denegado, descartando...~n", [JobId, Recursos]),
            remover_job(JobId);
            
        jobTimeout ->
            logF:log(msg,"[Job ~p] Timeout~n", [JobId, Recursos]),
            manejar_fallo(JobId, Recursos, Intentos)
    after ?TIMEOUT ->
        logF:log(msg,"[Job ~p] No hubo respuesta, descartando...~n", [JobId]),
        remover_job(JobId)
    end.

%si pasan mas de ciertos intentos, el trabajo se descarta
manejar_fallo(JobId, _ , ?INTENTOS_HASTA_DESCARTAR) ->
    logF:log(msg,"[Job ~p] se llego al limite de intentos, descartando...~n", [JobId]),
    remover_job(JobId);

%si hay un denied o un timeout espera
manejar_fallo(JobId, Recursos, Intentos) ->
    %%Si agarró un recurso, entonces lo suelta
    send_recv_manager:enviar_send({jobRelease, JobId}),
    SiguienteIntento = Intentos + 1,
    %% 1. Evitar Livelock: Dormir al proceso un tiempo exponencial con aleatoriedad
    aplicar_timeout(JobId, SiguienteIntento),
    
    %% 2. Reiniciar la máquina de estados desde la lista original limpia
    logF:log(msg,"[Job ~p] Reiniciando ciclo de peticiones (Intento ~p).~n", [JobId, SiguienteIntento]),
    adquirir_loop(JobId, Recursos, SiguienteIntento).

%formula para el timeout
aplicar_timeout(JobId, Intentos) ->
    TiempoBase = 150, 
    Limitar = lists:min([Intentos, 5]), 
    
    TiempoExponensial = TiempoBase * trunc(math:pow(2, Limitar)),
    Jitter = rand:uniform(300), 
    
    TiempoTotalSleep = TiempoExponensial + Jitter,
    logF:log(msg,"[Job ~p] Respetando backoff de ~p ms para mitigar contención.~n", [JobId, TiempoTotalSleep]),
    timer:sleep(TiempoTotalSleep).

enviar_estado_job(Status,JobId) ->
    case whereis(job_server) of 
        undefined -> logF:log(fatal,"El job_server no esta registrado~n");
        JobServerPid -> JobServerPid ! {find,Status,JobId}
    end.

agregar_job(JobPid,JobId) ->
    case whereis(job_server) of 
        undefined -> logF:log(fatal,"El job_server no esta registrado~n");
        JobServerPid -> JobServerPid ! {add,JobPid,JobId}
    end.
    

remover_job(JobId) ->
    case whereis(job_server) of 
        undefined -> logF:log(fatal,"El job_server no esta registrado~n");
        JobServerPid -> JobServerPid ! {remove,JobId}
    end.
    