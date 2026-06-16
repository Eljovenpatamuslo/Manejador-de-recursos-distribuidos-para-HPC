#include "recursos.h"

unsigned int obtener_cpus() {
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);

    if (cpus < 0) {
        perror("Error al obtener la cantidad de CPUs");
        return 1;
    }

    return (unsigned int) cpus;
}

unsigned long obtener_mem() {
    struct sysinfo info;

    if (sysinfo(&info) != 0) {
        perror("sysinfo");
        return 1;
    }

    return (unsigned long) info.totalram * info.mem_unit / (1024 * 1024);
}

unsigned int obtener_gpus() {
    struct dirent *de;
    DIR *dr = opendir("/sys/class/drm");
    if (dr == NULL) {
        return 0; 
    }

    unsigned int contador_gpus = 0;
    while ((de = readdir(dr)) != NULL) {
        if (strncmp(de->d_name, "card", 4) == 0 && strchr(de->d_name, '-') == NULL) {
            if (de->d_name[4] >= '0' && de->d_name[4] <= '9') {
                contador_gpus++;
            }
        }
    }
    closedir(dr);
    return contador_gpus;
}

RecursosNodo inicializar_recursos_locales() {
    RecursosNodo recursos = malloc(sizeof(struct _DatosNodo));
    assert(recursos != NULL);

    recursos->cpu = obtener_cpus();
    recursos->mem = obtener_mem();
    recursos->gpu = obtener_gpus();

    return recursos;
}