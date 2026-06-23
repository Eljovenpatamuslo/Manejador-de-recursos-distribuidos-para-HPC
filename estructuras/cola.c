#include "cola.h"

Cola cola_crear(){
    Cola cola = malloc(sizeof(struct _Cola));

    cola->primero = NULL; 
    cola->ultimo = NULL;

    return cola;
}

void cola_destruir(Cola cola){
    GNode *actual = cola->primero;
    while (actual != NULL) {
        GNode *sig = actual->sig;
        free(actual->dato);
        free(actual);
        actual = sig;
    }
    free(cola);
}

int cola_es_vacia(Cola cola){
    return cola->primero == NULL;
}

void* cola_inicio(Cola cola) {
    void *dato = NULL;

    if (cola->primero != NULL) {
        dato = cola->primero->dato;
    }

    return dato;
}

void cola_encolar(Cola cola, void* dato){
    GNode *nuevoNodo = malloc(sizeof(GNode));
	assert(nuevoNodo != NULL);

	nuevoNodo->sig = NULL;
	nuevoNodo->dato = dato;

    if (cola->primero == NULL) {
        cola->primero = nuevoNodo;
        cola->ultimo = nuevoNodo;
    } else {
        cola->ultimo->sig = nuevoNodo;
        cola->ultimo = nuevoNodo;
    }
}

void cola_desencolar(Cola cola){
    if (cola->primero != NULL) {
        GNode *temp = cola->primero;
        cola->primero = cola->primero->sig;

        if (cola->primero == NULL) {
            cola->ultimo = NULL;
        }

        free(temp);
    }
}
