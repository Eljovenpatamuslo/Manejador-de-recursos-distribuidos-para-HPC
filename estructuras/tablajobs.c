#include "tablajobs.h"
#include "recursos.h"

#define debug(str) fprintf(stderr, "HOLA: %s\n", str);
TablaJobs tablajobs_crear() {
    TablaJobs tabla = malloc(sizeof(struct _TablaJobs));
    assert(tabla != NULL);

    tabla->tablaPorId = calloc(MAX_JOBS, sizeof(JobActivo *));
    assert(tabla->tablaPorId != NULL);

    tabla->tablaPorNodo = calloc(MAX_NODOS, sizeof(JobActivo *));
    assert(tabla->tablaPorNodo != NULL);

    tabla->cantJobs = 0;
    pthread_mutex_init(&tabla->mutex, NULL);

    return tabla;
}

void tablajobs_destruir(TablaJobs tabla) {
    assert(tabla != NULL);

    for (int i = 0; i < MAX_JOBS; i++) {
        JobActivo *actual = tabla->tablaPorId[i];
        while (actual != NULL) {
            JobActivo *sig = actual->sigJobId;
            liberar_memoria_job(actual);
            actual = sig;
        }
    }

    free(tabla->tablaPorId);
    free(tabla->tablaPorNodo);

    pthread_mutex_destroy(&tabla->mutex);
    free(tabla);
}

void tablajobs_insertar(TablaJobs tabla, DatosJob *datos) {
    unsigned hashDatos = hash_ip(datos->nodoIp);

    pthread_mutex_lock(&tabla->mutex);

    // Calculamos la posicion de acuerdo a su jobId.
    unsigned idx = datos->jobId % MAX_JOBS;

    // Buscamos si el dato ya existe para evitar duplicados
    JobActivo *actual = tabla->tablaPorId[idx];
    while (actual != NULL) {
        if (actual->datos->jobId == datos->jobId) {
            int flag = 0;
            if (actual->datos->rol == JOB_REMOTO && datos->rol == JOB_REMOTO) {

                actual->datos->recReservados->cpu += datos->recReservados->cpu;
                actual->datos->recReservados->mem += datos->recReservados->mem;
                actual->datos->recReservados->gpu += datos->recReservados->gpu;

                flag = 1;
            } else if (actual->datos->rol == JOB_LOCAL &&
                       datos->rol == JOB_LOCAL &&
                       strcmp(actual->datos->nodoIp, datos->nodoIp) == 0 &&
                       actual->datos->nodoPuerto == datos->nodoPuerto &&
                       actual->datos->recPedidos == datos->recPedidos) {

                actual->datos->recReservados->cpu += datos->recReservados->cpu;
                actual->datos->recReservados->mem += datos->recReservados->mem;
                actual->datos->recReservados->gpu += datos->recReservados->gpu;

                actual->datos->recPedidos->cpu += datos->recPedidos->cpu;
                actual->datos->recPedidos->mem += datos->recPedidos->mem;
                actual->datos->recPedidos->gpu += datos->recPedidos->gpu;

                flag = 1;
            }

            if (flag) {
                pthread_mutex_unlock(&tabla->mutex);

                free(datos->recPedidos);
                free(datos->recReservados);
                free(datos);
                return; // No hay que agregar nuevos jobs
            }
        }
        actual = actual->sigJobId;
    }

    // Creamos el nodo
    JobActivo *nuevoJob = malloc(sizeof(JobActivo));
    assert(nuevoJob != NULL);
    nuevoJob->datos = datos;

    // Insertamos el job en la tabla de jobs por id
    nuevoJob->antJobId = NULL;
    nuevoJob->sigJobId = tabla->tablaPorId[idx];

    if (tabla->tablaPorId[idx] != NULL) {
        tabla->tablaPorId[idx]->antJobId = nuevoJob;
    }
    tabla->tablaPorId[idx] = nuevoJob;

    // Calculamos su posición de acuerdo al nodo asociado
    int nidx = hashDatos % MAX_NODOS;

    // Insertamos el job en la tabla de jobs por nodo
    nuevoJob->antJobNodo = NULL;
    nuevoJob->sigJobNodo = tabla->tablaPorNodo[nidx];

    if (tabla->tablaPorNodo[nidx] != NULL) {
        tabla->tablaPorNodo[nidx]->antJobNodo = nuevoJob;
    }
    tabla->tablaPorNodo[nidx] = nuevoJob;

    tabla->cantJobs++;

    pthread_mutex_unlock(&tabla->mutex);
}

void tablajobs_borrar_por_nodo(TablaJobs tabla, char ip[],
                               unsigned short puerto, RecursosNodo recursos) {
    unsigned idx = hash_ip(ip) % MAX_NODOS;
    JobActivo *jobsABorrar = NULL;

    pthread_mutex_lock(&tabla->mutex);

    JobActivo *actual = tabla->tablaPorNodo[idx];
    while (actual != NULL) {
        JobActivo *sig = actual->sigJobNodo;

        if (strcmp(ip, actual->datos->nodoIp) == 0 &&
            puerto == actual->datos->nodoPuerto) {
            desconectar_job(tabla, actual);
            tabla->cantJobs--;

            actual->antJobId = NULL;
            actual->antJobNodo = NULL;
            actual->sigJobNodo = NULL;

            actual->sigJobId = jobsABorrar;
            jobsABorrar = actual;
        }
        actual = sig;
    }

    pthread_mutex_unlock(&tabla->mutex);

    while (jobsABorrar != NULL) {
        JobActivo *sig = jobsABorrar->sigJobId;

        if (jobsABorrar->datos->rol == JOB_REMOTO) {
            liberar_recursos_reservados(recursos,
                                        jobsABorrar->datos->recReservados);
        }

        // Liberación final de memoria
        liberar_memoria_job(jobsABorrar);
        jobsABorrar = sig;
    }
}

unsigned long tablajobs_restar_recurso(TablaJobs tabla, unsigned long jobId,
                                       TipoRecurso rec) {
    assert(tabla != NULL);
    unsigned idx = jobId % MAX_JOBS;
    unsigned long cantLiberada = 0;

    pthread_mutex_lock(&tabla->mutex);

    JobActivo *actual = tabla->tablaPorId[idx];
    while (actual != NULL) {
        if (actual->datos->jobId == jobId && actual->datos->rol == JOB_REMOTO) {
            // Identificamos el recurso y guardamos cuánto tenía asignado
            if (rec == CPU) {
                cantLiberada = actual->datos->recReservados->cpu;
                actual->datos->recReservados->cpu = 0;
            } else if (rec == MEM) {
                cantLiberada = actual->datos->recReservados->mem;
                actual->datos->recReservados->mem = 0;
            } else if (rec == GPU) {
                cantLiberada = actual->datos->recReservados->gpu;
                actual->datos->recReservados->gpu = 0;
            }

            // Si ya no le queda ningún recurso asignado, lo borramos
            // físicamente
            if (actual->datos->recReservados->cpu == 0 &&
                actual->datos->recReservados->mem == 0 &&
                actual->datos->recReservados->gpu == 0) {

                desconectar_job(tabla, actual);
                tabla->cantJobs--;

                liberar_memoria_job(actual);
            }

            pthread_mutex_unlock(&tabla->mutex);
            return cantLiberada;
        }
        actual = actual->sigJobId;
    }

    pthread_mutex_unlock(&tabla->mutex);
    return 0; // No se encontró el job
}

int tablajobs_job_granted(TablaJobs tabla, unsigned long jobId) {
    int flag = 1;

    unsigned idx = jobId % MAX_JOBS;

    pthread_mutex_lock(&tabla->mutex);

    JobActivo *actual = tabla->tablaPorId[idx];
    while (actual != NULL) {
        if (actual->datos->jobId == jobId && actual->datos->rol == JOB_LOCAL) {
            flag &= comp_recursos(actual->datos->recPedidos,
                                  actual->datos->recReservados);
        }
        actual = actual->sigJobId;
    }

    pthread_mutex_unlock(&tabla->mutex);

    return flag;
}

void registrar_solicitud_propia(TablaJobs tablaJobs, unsigned long jobId,
                                TipoRecurso rec, unsigned long cant,
                                const char *ipDestino,
                                unsigned short puertoDestino,
                                void *datosCliente) {

    DatosJob *nuevosDatos = malloc(sizeof(DatosJob));
    assert(nuevosDatos != NULL);

    nuevosDatos->jobId = jobId;
    nuevosDatos->rol = JOB_LOCAL;
    nuevosDatos->datosCliente = datosCliente;
    strncpy(nuevosDatos->nodoIp, ipDestino, 16);
    nuevosDatos->nodoPuerto = puertoDestino;

    // Guardamos los recursos que pedimos a otros nodos
    nuevosDatos->recPedidos = malloc(sizeof(struct _RecursosReservados));
    nuevosDatos->recPedidos->cpu = rec == CPU ? cant : 0;
    nuevosDatos->recPedidos->mem = rec == MEM ? cant : 0;
    nuevosDatos->recPedidos->gpu = rec == GPU ? cant : 0;
    
    nuevosDatos->recursoPedido = rec;

    // Inicializamos como pendiente los recursos reservados
    nuevosDatos->recReservados = malloc(sizeof(struct _RecursosReservados));
    nuevosDatos->recReservados->cpu = 0;
    nuevosDatos->recReservados->mem = 0;
    nuevosDatos->recReservados->gpu = 0;

    tablajobs_insertar(tablaJobs, nuevosDatos);
}

void tablajobs_recurso_granted(TablaJobs tabla, unsigned long jobId, char ip[],
                               unsigned short puerto, TipoRecurso rec) {
    unsigned idx = jobId % MAX_JOBS;

    pthread_mutex_lock(&tabla->mutex);

    JobActivo *actual = tabla->tablaPorId[idx];
    while (actual != NULL) {
        if (actual->datos->jobId == jobId && actual->datos->rol == JOB_LOCAL &&
            strcmp(actual->datos->nodoIp, ip) == 0 &&
            actual->datos->nodoPuerto == puerto) {
            
            if (rec == CPU) {
                actual->datos->recReservados->cpu = actual->datos->recPedidos->cpu;
            } else if (rec == MEM) {
                actual->datos->recReservados->mem = actual->datos->recPedidos->mem;
            } else if (rec == GPU) {
                actual->datos->recReservados->gpu = actual->datos->recPedidos->gpu;
            }
            break;
        }
        actual = actual->sigJobId;
    }

    pthread_mutex_unlock(&tabla->mutex);
}

ListaResultados tablajobs_release_job(TablaJobs tabla, unsigned long jobId) {
    assert(tabla != NULL);

    unsigned idx = jobId % MAX_JOBS;

    ListaResultados resultadoPrimero = NULL;

    pthread_mutex_lock(&tabla->mutex);

    JobActivo *actual = tabla->tablaPorId[idx];
    while (actual != NULL) {
        JobActivo *sig = actual->sigJobId;

        if (actual->datos->jobId == jobId && actual->datos->rol == JOB_LOCAL) {
            struct _NodoResultado *nuevoNodo =
                malloc(sizeof(struct _NodoResultado));
            assert(nuevoNodo != NULL);

            nuevoNodo->datos = actual->datos;

            nuevoNodo->sig = resultadoPrimero;
            resultadoPrimero = nuevoNodo;

            desconectar_job(tabla, actual);
            tabla->cantJobs--;

            free(actual);
        }

        actual = sig;
    }

    pthread_mutex_unlock(&tabla->mutex);

    return resultadoPrimero;
}

void desconectar_job(TablaJobs tabla, JobActivo *job) {
    // Reajustamos los punteros de colisión de la tabla de jobs por id
    if (job->antJobId == NULL) {
        unsigned idx = job->datos->jobId % MAX_JOBS;
        tabla->tablaPorId[idx] = job->sigJobId;
    } else {
        job->antJobId->sigJobId = job->sigJobId;
    }

    if (job->sigJobId != NULL) {
        job->sigJobId->antJobId = job->antJobId;
    }

    // Reajustamos los punteros de colisión de la tabla de jobs por nodo
    if (job->antJobNodo == NULL) {
        unsigned nidx = hash_ip(job->datos->nodoIp) % MAX_NODOS;
        tabla->tablaPorNodo[nidx] = job->sigJobNodo;
    } else {
        job->antJobNodo->sigJobNodo = job->sigJobNodo;
    }

    if (job->sigJobNodo != NULL) {
        job->sigJobNodo->antJobNodo = job->antJobNodo;
    }

    job->antJobNodo = NULL;
    job->sigJobNodo = NULL;
    job->antJobId = NULL;
    job->sigJobId = NULL;
}

void liberar_memoria_job(JobActivo *job) {
    free(job->datos->recPedidos);
    free(job->datos->recReservados);
    free(job->datos);
    free(job);
}
