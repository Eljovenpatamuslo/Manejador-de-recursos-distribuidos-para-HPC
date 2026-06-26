#ifndef __RECURSOS_H__
#define __RECURSOS_H__

#include "cola.h"
#include "tablajobs.h"
#include "conexion.h"
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

typedef struct _NodoPromovido {
    unsigned long jobId;
    ClienteConexion *cliente;   // antes era int fd
    struct _NodoPromovido *sig;
} *ListaPromovidos;

/**
 * Inicializa los recursos vacios.
 */
Recurso inicializar_recurso(unsigned long capacidad);

/**
 * Retorna una estructura con la cantidad de cada recurso disponible.
 */
RecursosNodo inicializar_recursos_locales();

/**
 * Reserva un recurso en caso de que haya la cantidad disponible, en otro caso 
 * encola la solicitud.
 */
int reservar_recurso(RecursosNodo nodo, TablaJobs tablaJobs,
                     unsigned long jobId, TipoRecurso rec, unsigned long cant,
                     char ip[], unsigned short puerto, void *datosCliente);

/**
 * Libera el recurso del reservado por el job y atiende las solicitudes pendientes
 * en orden. Se devuelve una lista con las solicitudes que fueron atendidas.
 */
ListaPromovidos liberar_recurso(RecursosNodo nodo, TablaJobs tablaJobs,
                                unsigned long jobId, TipoRecurso rec);

/**
 * Libera los recursos dados, devolviendolos a la pool general.
 */
void liberar_recursos_reservados(RecursosNodo recursos,
                                 RecursosReservados reservados);

/**
 * Inicializa la estructura del recurso reservando la cantidad dada.
 */
RecursosReservados inicializar_recursos_reservados(TipoRecurso rec,
                                                   unsigned long cant);

/**
 * Compara si las dos instancias de recursos reservados coinciden en sus campos.
 */
int comp_recursos(RecursosReservados a, RecursosReservados b);

#endif /* __RECURSOS_H__ */
