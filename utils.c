#include "utils.h"

int cant_nucleos() {
    long num_cores = sysconf(_SC_NPROCESSORS_ONLN);

    if (num_cores == -1) {
        perror("Error al obtener cantidad de núcleos con sysconf");
        return 1;
    }

    return (int)num_cores;
}
