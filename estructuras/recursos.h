#ifndef __RECURSOS_H__
#define __RECURSOS_H__

#include "cola.h"
#include "tablajobs.h"
#include <assert.h>
#include <stdlib.h>

struct _Solicitud {
    unsigned long jobId;
    char ip[16];
    unsigned short puerto;
    unsigned long cant;
    void *datosCliente;
};
typedef struct _Solicitud *Solicitud;

struct _Recurso {
    unsigned long capacidad;
    unsigned long cantDisp;
    Cola solicitudPend;

    pthread_mutex_t mutex;
};
typedef struct _Recurso *Recurso;

struct _RecursosNodo {
    Recurso cpu;
    Recurso mem;
    Recurso gpu;
    pthread_mutex_t mutex;
};
typedef struct _RecursosNodo *RecursosNodo;

struct _NodoPromovido {
    unsigned long jobId;
    char ip[16];
    unsigned short puerto;
    int fd;
    struct _NodoPromovido *sig;
};
typedef struct _NodoPromovido *ListaPromovidos;

/**
 *
 */
Recurso inicializar_recurso(unsigned long capacidad);

/**
 * Retorna una estructura con la cantidad de cada recurso disponible.
 */
RecursosNodo inicializar_recursos_locales();

int reservar_recurso(RecursosNodo nodo, TablaJobs tablaJobs,
                     unsigned long jobId, TipoRecurso rec, unsigned long cant,
                     char ip[], unsigned short puerto, void *datosCliente);

ListaPromovidos liberar_recurso(RecursosNodo nodo, TablaJobs tablaJobs,
                                unsigned long jobId, TipoRecurso rec);

void liberar_recursos_reservados(RecursosNodo recursos,
                                 RecursosReservados reservados);

RecursosReservados inicializar_recursos_reservados(TipoRecurso rec,
                                                   unsigned long cant);

int comp_recursos(RecursosReservados a, RecursosReservados b);

#endif /* __RECURSOS_H__ */
