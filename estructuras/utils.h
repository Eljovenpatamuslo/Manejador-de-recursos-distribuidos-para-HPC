#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <dirent.h>
#include <string.h>

/**
 * Devuelve el hash asociado a la ip y puerto dados.
 */
unsigned int hash_ip_puerto(const char *ip, unsigned short puerto);

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

#endif /* __UTILS_H__ */