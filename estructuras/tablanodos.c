#include "tablanodos.h"

TablaNodos tablanodos_crear(unsigned capacidad) {

	// Pedimos memoria para la estructura principal y las casillas.
	TablaNodos tabla = malloc(sizeof(struct _TablaNodos));
	assert(tabla != NULL);
	tabla->nodos = calloc(capacidad, sizeof(NodoActivo*));
	assert(tabla->nodos != NULL);
	tabla->primerNodo = NULL;
	tabla->ultimoNodo = NULL;
	tabla->numNodos = 0;
	tabla->capacidad = capacidad;
    pthread_mutex_init(&tabla->mutex, NULL);

	return tabla;
}

void tablanodos_destruir(TablaNodos tabla) {
	assert(tabla != NULL);

	NodoActivo* actual = tabla->primerNodo;

	while(actual != NULL){
		NodoActivo* sig = actual->sigLista;
		
        free(actual->datos);
		free(actual);

		actual = sig;
	}

	// Liberar el arreglo de casillas, mutex y la tabla.
	free(tabla->nodos);
    pthread_mutex_destroy(&tabla->mutex);
	free(tabla);
}

void tablanodos_insertar(TablaNodos tabla, DatosNodo *datos) {

    unsigned hashDatos = hash(datos);

    pthread_mutex_lock(&tabla->mutex);

    if (((float)tabla->numNodos+1) / tabla->capacidad >= FACTORDECARGA) {
        tablanodos_redimensionar(tabla);
    }

	// Calculamos la posicion del datos dado, de acuerdo a la funcion hash.
    unsigned idx = hashDatos % tabla->capacidad;

    // Buscamos si el dato ya existe para evitar duplicados
    NodoActivo *actual = tabla->nodos[idx];
    while (actual != NULL) {
        if (comp(actual->datos, datos) == 0) {
            
            actual->ultimoAnuncio = time(NULL);

            pthread_mutex_unlock(&tabla->mutex);
            return; // No hay que agregar nuevos nodos
        }
        actual = actual->sigHash;
    }

    // Creamos el nodo
    NodoActivo *nuevoNodo = malloc(sizeof(NodoActivo));
    assert(nuevoNodo != NULL);
    
	nuevoNodo->datos = datos;
    nuevoNodo->ultimoAnuncio = time(NULL);
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

    pthread_mutex_unlock(&tabla->mutex);
}

void tablanodos_borrar_expirados(TablaNodos tabla) {
    assert(tabla != NULL);

    time_t tiempoActual = time(NULL);
    NodoActivo *nodosExpirados = NULL; 

    pthread_mutex_lock(&tabla->mutex);
    NodoActivo *actual = tabla->primerNodo;

    // Recorremos la lista secuencialmente
    while (actual != NULL) {
        NodoActivo *sig = actual->sigLista; 
        
        if (tiempoActual - actual->ultimoAnuncio > 15) {
            
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
                // Nota: Asegúrate de que 'hash(actual->datos)' sea inmutable e invariante aquí
                unsigned idx = hash(actual->datos) % tabla->capacidad;
                tabla->nodos[idx] = actual->sigHash;
            } else {
                actual->antHash->sigHash = actual->sigHash;
            }

            if (actual->sigHash != NULL) {
                actual->sigHash->antHash = actual->antHash;
            }

            tabla->numNodos--;
            
            actual->sigHash = nodosExpirados;
            nodosExpirados = actual;
        }

        actual = sig;
    }
    
    pthread_mutex_unlock(&tabla->mutex);

    while (nodosExpirados != NULL) {
        NodoActivo *sig = nodosExpirados->sigHash;
        free(nodosExpirados->datos);
        free(nodosExpirados);
        nodosExpirados = sig;
    }
}

static void tablanodos_redimensionar(TablaNodos tabla){
    unsigned nuevaCapacidad = 2 * tabla->capacidad;
    NodoActivo **nuevosNodos = calloc(nuevaCapacidad, sizeof(NodoActivo*));
    assert(nuevosNodos != NULL);

    // Recorremos la tabla como lista
    NodoActivo *actual = tabla->primerNodo;
    while (actual != NULL) {
        unsigned nuevoIdx = hash(actual->datos) % nuevaCapacidad;

        actual->antHash = NULL;
        actual->sigHash = nuevosNodos[nuevoIdx];
        
        if (nuevosNodos[nuevoIdx] != NULL) {
            nuevosNodos[nuevoIdx]->antHash = actual;
        }
        nuevosNodos[nuevoIdx] = actual;

        actual = actual->sigLista;
    }

    free(tabla->nodos);
    tabla->nodos = nuevosNodos;
    tabla->capacidad = nuevaCapacidad;
}