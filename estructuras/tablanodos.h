#ifndef __TABLANODOS_H__
#define __TABLANODOS_H__

typedef struct _NodoActivo {
    void* dato;

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
void tablanodos_insertar(TablaNodos tabla, void *dato);

#endif /* __TABLANODOS_H__ */