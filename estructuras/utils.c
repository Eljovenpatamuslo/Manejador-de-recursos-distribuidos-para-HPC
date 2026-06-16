#include "utils.h"

unsigned int hash_ip_puerto(const char *ip, unsigned short puerto) {
    unsigned int hash = 5381;
    int c;

    while ((c = *ip++)) {
        hash = ((hash << 5) + hash) + c;
    }

    hash = ((hash << 5) + hash) + puerto;

    return hash;
}