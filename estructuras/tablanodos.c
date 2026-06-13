#include "tablanodos.h"
#include <assert.h>
#include <stdlib.h>
#include <time.h>

TablaNodos tablanodos_crear(unsigned capacidad) {

	// Pedimos memoria para la estructura principal y las casillas.
	TablaNodos tabla = malloc(sizeof(struct _TablaNodos));
	assert(tabla != NULL);
	tabla->nodos = malloc(sizeof(NodoActivo*) * capacidad);
	assert(tabla->nodos != NULL);
	tabla->primerNodo = NULL;
	tabla->ultimoNodo = NULL;
	tabla->numNodos = 0;
	tabla->capacidad = capacidad;

	// Inicializamos las casillas con datos nulos.
	for (unsigned idx = 0; idx < capacidad; ++idx) {
		tabla->nodos[idx] = NULL;
	}

	return tabla;
}

void tablanodos_destruir(TablaNodos tabla) {
	assert(tabla != NULL);

	NodoActivo* actual = tabla->primerNodo;

	while(actual != NULL){
		NodoActivo* sig = actual->sigLista;
		
        dest(actual->dato);
		free(actual);

		actual = sig;
	}

	// Liberar el arreglo de casillas y la tabla.
	free(tabla->nodos);
	free(tabla);
	return;
}

void tablanodos_insertar(TablaNodos tabla, void *dato) {

    if ((float)tabla->numNodos / tabla->capacidad >= FACTORDECARGA) {
        tablanodos_redimensionar(tabla);
    }

	// Calculamos la posicion del dato dado, de acuerdo a la funcion hash.
    unsigned idx = hash(dato) % tabla->capacidad;

    // Buscamos si el dato ya existe para evitar duplicados
    NodoActivo *actual = tabla->nodos[idx];
    while (actual != NULL) {
        if (comparar_ip(actual->dato, dato) == 0) { // Comparar por ip
            
            actualizar_timestamp(actual->dato); // Actualizamos el timestamp del nodo
            return; // No hay que agregar nuevos nodos
        }
        actual = actual->sigHash;
    }

    // Si no existe, creamos el nodo
    NodoActivo *nuevoNodo = malloc(sizeof(NodoActivo));
    assert(nuevoNodo != NULL);
    
	nuevoNodo->dato = dato;
    nuevoNodo->antHash = NULL;

    if(tabla->nodos[idx] != NULL){
        tabla->nodos[idx]->antHash = nuevoNodo;
    }
    nuevoNodo->sigHash = tabla->nodos[idx];
    tabla->nodos[idx] = nuevoNodo;

    // Insertar al final de la lista doblemente enlazada
    nuevoNodo->sigLista = NULL;
    if (tabla->numNodos == 0) {
        nuevoNodo->antLista = NULL;
        tabla->primerNodo = nuevoNodo;
        tabla->ultimoNodo = nuevoNodo;
    } else {
        nuevoNodo->antLista = tabla->ultimoNodo;
        tabla->ultimoNodo->sigLista = nuevoNodo;
        tabla->ultimoNodo = nuevoNodo;
    }

    tabla->numNodos++;
}

void tablanodos_borrar_expirados(TablaNodos tabla){
	assert(tabla != NULL);

    time_t tiempo_actual = time(NULL);
    NodoActivo *actual = tabla->primerNodo;

    // Recorremos la lista global secuencialmente
    while (actual != NULL) {
        NodoActivo *sig = actual->sigLista; 

        if (tiempo_actual - ultimo_anuncio(actual->dato) > 15) {
            
            // Reajustamos los punteros de la lista
            if (actual->antLista == NULL) {
                tabla->primerNodo = actual->sigLista;
            } else {
                actual->antLista->sigLista = actual->sigLista;
            }

            if (actual->sigLista == NULL) {
                tabla->ultimoNodo = actual->antLista;
            } else {
                actual->sigLista->antLista = actual->antLista;
            }

            // Reajustamos los punteros de colisión del hash
            if (actual->antHash == NULL) {
                unsigned idx = hash(actual->dato) % tabla->capacidad;
                tabla->nodos[idx] = actual->sigHash;
            } else {
                actual->antHash->sigHash = actual->sigHash;
            }

            if (actual->sigHash != NULL) {
                actual->sigHash->antHash = actual->antHash;
            }

            destr(actual->dato);
            free(actual);
            
            tabla->numNodos--;
        }

        actual = sig;
    }
}