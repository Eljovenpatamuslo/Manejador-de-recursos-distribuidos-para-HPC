#include "tablanodos.h"

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
		
        destr(actual->dato);
		free(actual);

		actual = sig;
	}

	// Liberar el arreglo de casillas y la tabla.
	free(tabla->nodos);
	free(tabla);
	return;
}

void tablanodos_insertar(TablaNodos tabla, void *dato) {

	// Calculamos la posicion del dato dado, de acuerdo a la funcion hash.
    unsigned idx = hash(dato) % tabla->capacidad;

    // Buscamos si el dato ya existe para evitar duplicados
    NodoActivo *actual = tabla->nodos[idx];
    while (actual != NULL) {
        if (comp(actual->dato, dato) == 0) {
            
            actualizar_timestamp(actual->dato);
            return; // No hay que agregar nuevos nodos
        }
        actual = actual->sigHash;
    }

    // Si no existe vemos si es necesario redimensionar
    if (((float)tabla->numNodos+1) / tabla->capacidad >= FACTORDECARGA) {
        tablanodos_redimensionar(tabla);

        // Recalculamos la posicion del dato
        idx = hash(dato) % tabla->capacidad;
    }

    // Creamos el nodo
    NodoActivo *nuevoNodo = malloc(sizeof(NodoActivo));
    assert(nuevoNodo != NULL);
    
	nuevoNodo->dato = dato;
    nuevoNodo->antHash = NULL;
    nuevoNodo->sigHash = tabla->nodos[idx];

    if(tabla->nodos[idx] != NULL){
        tabla->nodos[idx]->antHash = nuevoNodo;
    }
    
    tabla->nodos[idx] = nuevoNodo;

    // Insertar al final de la lista doblemente enlazada
    if (tabla->numNodos == 0) {
        nuevoNodo->antLista = NULL;
        tabla->primerNodo = nuevoNodo;
    } else {
        nuevoNodo->antLista = tabla->ultimoNodo;
        tabla->ultimoNodo->sigLista = nuevoNodo;
    }
    nuevoNodo->sigLista = NULL;
    tabla->ultimoNodo = nuevoNodo;

    tabla->numNodos++;
}

void tablanodos_borrar_expirados(TablaNodos tabla){
	assert(tabla != NULL);

    time_t tiempo_actual = time(NULL);
    NodoActivo *actual = tabla->primerNodo;

    // Recorremos la lista secuencialmente
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

void tablanodos_redimensionar(TablaNodos tabla){
    unsigned nueva_capacidad = 2 * tabla->capacidad;
    NodoActivo **nuevos_nodos = malloc(sizeof(NodoActivo*) * nueva_capacidad);
    assert(nuevos_nodos != NULL);

    // Inicializar el nuevo arreglo con NULL
    for (unsigned idx = 0; idx < nueva_capacidad; ++idx) {
        nuevos_nodos[idx] = NULL;
    }

    // Recorremos la tabla como lista
    NodoActivo *actual = tabla->primerNodo;
    while (actual != NULL) {
        unsigned nuevo_idx = hash(actual->dato) % nueva_capacidad;

        actual->antHash = NULL;
        actual->sigHash = nuevos_nodos[nuevo_idx];
        
        if (nuevos_nodos[nuevo_idx] != NULL) {
            nuevos_nodos[nuevo_idx]->antHash = actual;
        }
        nuevos_nodos[nuevo_idx] = actual;

        actual = actual->sigLista;
    }

    free(tabla->nodos);
    tabla->nodos = nuevos_nodos;
    tabla->capacidad = nueva_capacidad;
}