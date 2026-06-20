#include "cola.h"

Cola cola_crear(){
    Cola cola = malloc(sizeof(struct _Cola));

    cola->primero = NULL; 
    cola->ultimo = NULL;

    return cola;
}

void cola_destruir(Cola cola){
    pthread_mutex_lock(&cola->mutex);
    GNode *actual = cola->primero;
    while (actual != NULL) {
        free(actual->dato);
        free(actual);
    }
    pthread_mutex_unlock(&cola->mutex);
    pthread_mutex_destroy(&cola->mutex);
}

int cola_es_vacia(Cola cola){
    int flag;
    pthread_mutex_lock(&cola->mutex);   
    flag = cola->primero == NULL;
    pthread_mutex_unlock(&cola->mutex);
    return flag;
}

void* cola_desencolar(Cola cola){
    void *dato;

    pthread_mutex_lock(&cola->mutex);

    dato = cola->primero->dato;
    cola->primero = cola->primero->sig;
    if (cola->primero == NULL) {
        cola->ultimo = NULL;
    }

    return dato;
}

void cola_encolar(Cola cola, void* dato){
    GNode *nuevoNodo = malloc(sizeof(GNode));
	assert(nuevoNodo != NULL);

	nuevoNodo->sig = cola->primero;
	nuevoNodo->dato = dato;
    if (cola->ultimo == NULL) {
        cola->ultimo = nuevoNodo;
    }
}

