#ifndef __COLA_H__
#define __COLA_H__

#include <assert.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>

typedef void (*FuncionDestructora)(void *dato);
typedef void *(*FuncionCopia)(void *dato);
typedef void (*FuncionVisitante)(void *dato);
typedef int (*Predicado)(void *dato);
typedef int (*FuncionComparadora)(void *, void *);

typedef struct _GNode {
    void *dato;
    struct _GNode *sig;
} GNode;

typedef struct _Cola {
    GNode *primero;
    GNode *ultimo;
} *Cola;

/**
 * Crea una cola vacia.
 */
Cola cola_crear();

/**
 * Destruye la cola y libera la memoria.
 */
void cola_destruir(Cola);

/**
 * Verifica si la cola es vacia.
 */
int cola_es_vacia(Cola);

/**
 * Retorna el primer elemento de la cola.
 */
void *cola_inicio(Cola);

/**
 * Encola el elemento dado en la cola.
 */
void cola_encolar(Cola, void *);

/**
 * Desencola el primer elemento de la cola.
 */
void cola_desencolar(Cola);

#endif
