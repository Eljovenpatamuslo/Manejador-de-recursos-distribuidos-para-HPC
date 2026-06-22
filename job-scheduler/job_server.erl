%forma de mandarle los datos
% [IP:<ip>:PUERTO:<puerto>,CPU:<Ncpu>,MEM,<Nmem>,(GPU:<Ngpu> opcional)]

% ejemplo: NODES 192.168.1.10:8100:cpu:4:mem:8192:gpu:1;192.168.1.11:8101:cpu:2:mem:4096

-module(job_server).

-export([iniciar_job_server/0,job_server/1,crear_job/1,
    ejecutar_trabajo/1,adquirir_loop/3,manejar_fallo/3,
    aplicar_timeout/2,enviar_estado_job/2]).

-define(SEC, 1000).
-define(TIMEOUT, 1000 * ?SEC).

iniciar_job_server() ->
    ServerPid = spawn_link(?MODULE,job_server,[maps:new()]),
    register(job_server,ServerPid),
    ok.

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

crear_job(Resource) -> 
    JobId = erlang:unique_integer([positive]),
    JobPid = spawn(?MODULE,adquirir_loop,[JobId,Resource,0]),
    job_server ! {add,JobPid,JobId}.

ejecutar_trabajo(JobId) ->
    logF:log(msg,"[Job ~p] Ejecutando procesamiento en el cluster simulado...~n", [JobId]),
    timer:sleep(5 * ?SEC),
    
    logF:log(msg,"[Job ~p] Procesamiento completado. Liberando infraestructura.~n", [JobId]),
    send_manager ! {jobRelease,JobId},
    job_server ! {remove,JobId},
    logF:log(msg,"[Job ~p] Terminado correctamente.~n", [JobId]),
    ok.

adquirir_loop(JobId, Recursos, Intentos) ->
    logF:log(msg,"[Job ~p] Solicitando los recursos: ~p~n", [JobId, Recursos]),
    
    send_recv_manager:enviar_send({jobRequest, JobId, Recursos}),
    
    receive
        jobGranted -> 
           logF:log(msg,"[Job ~p] EXITO! Todos los recursos concedidos.~n", [JobId]),
            ejecutar_trabajo(JobId);
            
        jobDenied ->
            logF:log(msg,"[Job ~p] ALERTA: job denegado (~p). Iniciando aborto...~n", [JobId, Recursos]),
            manejar_fallo(JobId, Recursos, Intentos);
            
        jobTimeout ->
            logF:log(msg,"[Job ~p] ALERTA: Timeout en red por recursos (~p).~n", [JobId, Recursos]),
            manejar_fallo(JobId, Recursos, Intentos);
        closed -> closed
    after ?TIMEOUT ->
        logF:log(msg,"[Job ~p] ERROR CRITICO: Timeout interno del planificador.~n", [JobId]),
        manejar_fallo(JobId, Recursos, Intentos)
    end.

manejar_fallo(JobId, Recursos, Intentos) ->
    SiguienteIntento = Intentos + 1,
    %% 1. Evitar Livelock: Dormir al proceso un tiempo exponencial con aleatoriedad
    aplicar_timeout(JobId, SiguienteIntento),
    
    %% 2. Reiniciar la máquina de estados desde la lista original limpia
    logF:log(msg,"[Job ~p] Reiniciando ciclo de peticiones (Intento ~p).~n", [JobId, SiguienteIntento]),
    adquirir_loop(JobId, Recursos, SiguienteIntento).



aplicar_timeout(JobId, Intentos) ->

    TiempoBase = 150, 
    Limitar = lists:min([Intentos, 5]), 
    
    TiempoExponensial = TiempoBase * trunc(math:pow(2, Limitar)),
    Jitter = rand:uniform(300), 
    
    TiempoTotalSleep = TiempoExponensial + Jitter,
    logF:log(msg,"[Job ~p] Respetando backoff de ~p ms para mitigar contención.~n", [JobId, TiempoTotalSleep]),
    timer:sleep(TiempoTotalSleep).

enviar_estado_job(Status,JobId) ->
    job_server ! {find,Status,JobId}.
