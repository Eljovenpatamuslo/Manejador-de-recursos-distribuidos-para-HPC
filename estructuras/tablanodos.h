#ifndef __TABLANODOS_H__
#define __TABLANODOS_H__

#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <stdint.h>

typedef struct _DatosNodo {
    char ip[16];
    uint16_t puerto;
    unsigned cpu;
    unsigned mem;
    unsigned gpu;
} DatosNodo;

typedef struct _NodoActivo {
    DatosNodo *datos;
    time_t ultimoAnuncio;

    struct _NodoActivo *antHash;
	struct _NodoActivo *sigHash;

	struct _NodoActivo *antLista;
	struct _NodoActivo *sigLista;
} NodoActivo;

struct _TablaNodos {
    NodoActivo **nodos;
	NodoActivo *primerNodo;
	NodoActivo *ultimoNodo;

    unsigned numNodos;
    unsigned capacidad;

    pthread_mutex_t mutex;
};

typedef struct _TablaNodos *TablaNodos;

#define FACTORDECARGA 0.75
#define TIEMPODEANUNCIO 15

/**
 * Crea una nueva tabla de nodos vacia, con la capacidad dada.
 */
TablaNodos tablanodos_crear(unsigned capacidad);

/**
 * Destruye la tabla.
 */
void tablanodos_destruir(TablaNodos tabla);

/**
 * Inserta un nodo en la tabla, o actualiza su timestamp si ya se encontraba.
 */
void tablanodos_insertar(TablaNodos tabla, DatosNodo *dato);

/**
 * Borra todos los nodos que no se hayan anunciado nuevamente antes de que pase
 * el tiempo de expiración.
 */
static void tablanodos_borrar_expirados(TablaNodos tabla);

/**
 * Duplica el tamaño de la tabla rehasheando todos los nodos.
 */
void tablanodos_redimensionar(TablaNodos tabla);

#endif /* __TABLANODOS_H__ */