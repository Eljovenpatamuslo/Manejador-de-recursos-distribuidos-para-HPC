#ifndef __RECURSOS_H__
#define __RECURSOS_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <dirent.h>
#include <string.h>
#include <assert.h>

struct _RecursosNodo {
    unsigned int cpu;
    unsigned long mem;
    unsigned int gpu;
};

typedef struct _RecursosNodo* RecursosNodo;

/**
 * Retorna la cantidad de cpus que tiene la computadora.
 */
unsigned int obtener_cpus();

/**
 * Retorna la cantidad de memoria en MB que tiene la computadora.
 */
unsigned long obtener_mem();

/**
 * Retorna la cantidad de placas de video que tiene la computadora.
 */
unsigned int obtener_gpus();

/**
 * Retorna una estructura con la cantidad de cada recurso disponible.
 */
RecursosNodo inicializar_recursos_locales();

#endif /* __RECURSOS_H__ */