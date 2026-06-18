#ifndef __TABLAJOBS_H_
#define __TABLAJOBS_H_

#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#include "utils.h"

#define MAX_JOBS 99991
#define MAX_NODOS 503

typedef struct _DatosJob {
    unsigned long jobId;
    char nodoIp[16];
    unsigned short nodoPuerto;
    char recursosReservados[128];
} DatosJob;

typedef struct _JobActivo {
    DatosJob *datos;

    struct _JobActivo *antJobId;
    struct _JobActivo *sigJobId; 

    struct _JobActivo *antJobNodo;
    struct _JobActivo *sigJobNodo;
} JobActivo;

typedef struct _TablaJobs {
    JobActivo **tablaPorId;
    JobActivo **tablaPorNodo;

    pthread_mutex_t mutex;

    unsigned int cantJobs;
} *TablaJobs;

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
 * Borra un job de la tabla según su id.
 */
void tablajobs_borrar_por_id(TablaJobs tabla, unsigned long jobId);

/**
 * Borra todos los jobs asociados a una misma ip y puerto (nodo).
 */
void tablajobs_borrar_por_nodo(TablaJobs tabla, char ip[], unsigned short puerto);

/**
 * Redirecciona los punteros de la tabla para desconectar el job.
 */
static void desconectar_job(TablaJobs tabla, JobActivo* job);

#endif /* _TABLAJOBS_H_ */