#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdlib.h>

/**
 * Devuelve el hash asociado a la ip y puerto dados.
 */
unsigned int hash_ip_puerto(const char *ip, unsigned short puerto);

#endif /* __UTILS_H__ */