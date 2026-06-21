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

Cola cola_crear();

void cola_destruir(Cola);

int cola_es_vacia(Cola);

void *cola_inicio(Cola);

void cola_encolar(Cola, void *);

void cola_desencolar(Cola);

#endif
