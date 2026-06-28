%forma de mandarle los datos
% [IP:<ip>:PUERTO:<puerto>,CPU:<Ncpu>,MEM,<Nmem>,(GPU:<Ngpu> opcional)]

%NODES 192.168.1.10:8100:cpu:4:mem:8192:gpu:1;192.168.1.11:8101:cpu:2:mem:4096

-module(job_scheduler).

-export([scheduler_init/1,scheduler/0,get_pid_scheduler/0]).
-define(SEC, 1000).
-define(TIME_BEFORE_DOING_MORE_JOBS, 10* ?SEC).

%iniciar todas los modulos y el scheduler en si
scheduler_init(PuertoC) ->
    ok = logF:crear_error_managment(),
    logF:log(msg,"Log iniciado ~n"),
    
    ok = send_recv_manager:send_recv_init(PuertoC),
    logF:log(msg,"send_recv_manager iniciado ~n"),

    ok = job_server:iniciar_job_server(),
    logF:log(msg,"job_server iniciado~n"),

    register(scheduler,self()),
    scheduler().

%el scheduler pide los nodos del server, crea trabajos, espera a que esten todos completados, espera un tiempo y vuelve a repetir
scheduler() ->
    ListaNodos = manejador_recursos:obtener_y_formatear_nodos(),    
    logF:log(msg,"nodos: ~p~n", [ListaNodos]),
    Recursos = manejador_recursos:obtener_recursos_para_jobs(ListaNodos),
    lists:foreach(fun(Recurso) -> job_server:crear_job(Recurso) end,Recursos),
    receive
        alljobsdone -> ok
    end,
    timer:sleep(?TIME_BEFORE_DOING_MORE_JOBS),
    scheduler().

%obtiene el id de proceso del scheduler
get_pid_scheduler() ->
    case whereis(scheduler) of
        undefined -> logF:log(fatal,"scheduler no esta registrado"),undefined;
        SchedulerPid -> SchedulerPid
    end.
