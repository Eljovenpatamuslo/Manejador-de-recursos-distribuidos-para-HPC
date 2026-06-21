#include "tablanodos.h"

TablaNodos tablanodos_crear(unsigned capacidad) {
    // Pedimos memoria para la estructura principal y las casillas.
    TablaNodos tabla = malloc(sizeof(struct _TablaNodos));
    assert(tabla != NULL);

    tabla->nodos = calloc(capacidad, sizeof(NodoActivo *));
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

    NodoActivo *actual = tabla->primerNodo;

    while (actual != NULL) {
        NodoActivo *sig = actual->sigLista;

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
    unsigned hashDatos = hash_ip_puerto(datos->ip, datos->puerto);

    pthread_mutex_lock(&tabla->mutex);

    if (((float)tabla->numNodos + 1) / tabla->capacidad >= FACTORDECARGA) {
        tablanodos_redimensionar(tabla);
    }

    // Calculamos la posicion del datos dado, de acuerdo a la funcion hash.
    unsigned idx = hashDatos % tabla->capacidad;

    // Buscamos si el dato ya existe para evitar duplicados
    NodoActivo *actual = tabla->nodos[idx];
    while (actual != NULL) {
        if (comp_nodos(actual->datos, datos) == 0) {
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

    // Insertamos en la tabla hash
    nuevoNodo->antHash = NULL;
    nuevoNodo->sigHash = tabla->nodos[idx];

    if (tabla->nodos[idx] != NULL) {
        tabla->nodos[idx]->antHash = nuevoNodo;
    }
    tabla->nodos[idx] = nuevoNodo;

    // Insertamos al final de la lista doblemente enlazada
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

void tablanodos_borrar_expirados(TablaNodos tablaNodos, TablaJobs tablaJobs,
                                 RecursosNodo recursos) {
    assert(tablaNodos != NULL);

    time_t tiempoActual = time(NULL);
    NodoActivo *nodosExpirados = NULL;

    pthread_mutex_lock(&tablaNodos->mutex);
    NodoActivo *actual = tablaNodos->primerNodo;

    // Recorremos la lista secuencialmente
    while (actual != NULL) {
        NodoActivo *sig = actual->sigLista;

        if (tiempoActual - actual->ultimoAnuncio > 15) {
            desconectar_nodo(tablaNodos, actual);

            actual->sigHash = nodosExpirados;
            nodosExpirados = actual;
        }

        actual = sig;
    }

    pthread_mutex_unlock(&tablaNodos->mutex);

    while (nodosExpirados != NULL) {
        NodoActivo *sig = nodosExpirados->sigHash;
        tablajobs_borrar_por_nodo(tablaJobs, nodosExpirados->datos->ip,
                                  nodosExpirados->datos->puerto, recursos);
        free(nodosExpirados->datos);
        free(nodosExpirados);
        nodosExpirados = sig;
    }
}

void tablanodos_redimensionar(TablaNodos tabla) {
    unsigned nuevaCapacidad = 2 * tabla->capacidad;
    NodoActivo **nuevosNodos = calloc(nuevaCapacidad, sizeof(NodoActivo *));
    assert(nuevosNodos != NULL);

    // Recorremos la tabla como lista
    NodoActivo *actual = tabla->primerNodo;
    while (actual != NULL) {
        unsigned nuevoIdx =
            hash_ip_puerto(actual->datos->ip, actual->datos->puerto) %
            nuevaCapacidad;

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

void desconectar_nodo(TablaNodos tabla, NodoActivo *nodo) {
    // Reajustamos los punteros de la lista
    if (nodo->antLista == NULL) {
        tabla->primerNodo = nodo->sigLista;
    } else {
        nodo->antLista->sigLista = nodo->sigLista;
    }

    if (nodo->sigLista == NULL) {
        tabla->ultimoNodo = nodo->antLista;
    } else {
        nodo->sigLista->antLista = nodo->antLista;
    }

    // Reajustamos los punteros de colisión del hash
    if (nodo->antHash == NULL) {
        unsigned idx = hash_ip_puerto(nodo->datos->ip, nodo->datos->puerto) %
                       tabla->capacidad;
        tabla->nodos[idx] = nodo->sigHash;
    } else {
        nodo->antHash->sigHash = nodo->sigHash;
    }

    if (nodo->sigHash != NULL) {
        nodo->sigHash->antHash = nodo->antHash;
    }

    tabla->numNodos--;
}

int comp_nodos(const DatosNodo *a, const DatosNodo *b) {
    if (a->puerto != b->puerto) {
        return (a->puerto < b->puerto) ? -1 : 1;
    }

    return strcmp(a->ip, b->ip);
}
