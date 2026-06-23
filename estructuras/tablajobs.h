#ifndef __TABLAJOBS_H_
#define __TABLAJOBS_H_

#include "utils.h"
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

#define MAX_JOBS 99991
#define MAX_NODOS 503

typedef enum { CPU, MEM, GPU } TipoRecurso;

typedef enum { JOB_LOCAL, JOB_REMOTO } RolJob;

struct _RecursosReservados {
    unsigned int cpu;
    unsigned long mem;
    unsigned int gpu;
};
typedef struct _RecursosReservados *RecursosReservados;

typedef struct _DatosJob {
    unsigned long jobId;
    RolJob rol;
    void *datosCliente;
    char nodoIp[16];
    unsigned short nodoPuerto;
    RecursosReservados recReservados;
    RecursosReservados recPedidos;
} DatosJob;

typedef struct _JobActivo {
    DatosJob *datos;

    struct _JobActivo *antJobId;
    struct _JobActivo *sigJobId;

    struct _JobActivo *antJobNodo;
    struct _JobActivo *sigJobNodo;
} JobActivo;

struct _TablaJobs {
    JobActivo **tablaPorId;
    JobActivo **tablaPorNodo;

    pthread_mutex_t mutex;

    unsigned int cantJobs;
};
typedef struct _TablaJobs *TablaJobs;

struct _RecursosNodo;
typedef struct _RecursosNodo *RecursosNodo;

typedef struct _NodoResultado {
    DatosJob *datos;
    struct _NodoResultado *sig;
} *ListaResultados;

/**
 * Crea una nueva tabla de jobs vacia.
 */
TablaJobs tablajobs_crear();

/**
 * Destruye la tabla.
 */
void tablajobs_destruir(TablaJobs tabla);

/**
 * Inserta un job en la tabla si no se encontraba.
 */
void tablajobs_insertar(TablaJobs tabla, DatosJob *datos);

/**
 * Borra todos los jobs asociados a una misma ip y puerto (nodo).
 */
void tablajobs_borrar_por_nodo(TablaJobs tabla, char ip[],
                               unsigned short puerto, RecursosNodo recursos);

/**
 * Resta el recurso dado del job (identificado por el job id) y retorna lo restado.
 */
unsigned long tablajobs_restar_recurso(TablaJobs tabla, unsigned long jobId,
                                       TipoRecurso rec);

/**
 * Verifica que la cantidad de recursos pedidos del job coincidan con la cantidad
 * de recursos concedidos (reservados).
 */
int tablajobs_job_granted(TablaJobs tabla, unsigned long jobId);

/**
 * Guarda en la tabla reservas hechas a otros nodos, guardando que se pidió y a
 * quién se pidió. 
 */
void registrar_solicitud_propia(TablaJobs tablaJobs, unsigned long jobId,
                                TipoRecurso rec, unsigned long cant,
                                const char *ipDestino,
                                unsigned short puertoDestino,
                                void *datosCliente);

/**
 * Se guarda el recurso como reservado al recibir una confirmación de una petición
 * a un nodo remoto.
 */
void tablajobs_recurso_granted(TablaJobs tabla, unsigned long jobId, char ip[],
                               unsigned short puerto);

/**
 * Libera los recursos asociados al job. Retorna una lista de todas las reservas
 * hechas a nodos remotos.
 */
ListaResultados tablajobs_release_job(TablaJobs tabla, unsigned long jobId);

/**
 * Desconecta el job de la tabla.
 */
void desconectar_job(TablaJobs tabla, JobActivo *job);

/**
 * Libera la memoria de los datos del job.
 */
void liberar_memoria_job(JobActivo *job);

#endif /* _TABLAJOBS_H_ */
