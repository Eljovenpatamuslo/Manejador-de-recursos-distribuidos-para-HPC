#ifndef __TABLANODOS_H__
#define __TABLANODOS_H__

#include "tablajobs.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct _DatosNodo {
    char ip[16];
    unsigned short puerto;
    char recursos[128];
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
void tablanodos_borrar_expirados(TablaNodos tablaNodos, TablaJobs tablaJobs,
                                 RecursosNodo recursos);

/**
 * Duplica el tamaño de la tabla rehasheando todos los nodos.
 */
void tablanodos_redimensionar(TablaNodos tabla);

/**
 * Desconecta el nodo de la tabla de nodos.
 */
void desconectar_nodo(TablaNodos tabla, NodoActivo *nodo);

/**
 * Compara si los nodos coinciden en la ip.
 */
int comp_nodos(const DatosNodo *a, const DatosNodo *b);

/**
 * Busca un nodo por su ip y retorna sus datos.
 */
DatosNodo *tablanodos_buscar(TablaNodos tabla, char ip[]);

/**
 * Retorna un string que contiene los datos de todos los nodos de la tabla. 
 */
char *tablanodos_obtener_nodos(TablaNodos tabla);

#endif /* __TABLANODOS_H__ */
