#include "recursos.h"

Recurso inicializar_recurso(unsigned long capacidad) {
    Recurso recurso = malloc(sizeof(struct _Recurso));
    assert(recurso != NULL);

    recurso->capacidad = capacidad;
    recurso->cantDisp = capacidad;
    recurso->solicitudPend = cola_crear();
    pthread_mutex_init(&recurso->mutex, NULL);

    return recurso;
}

RecursosNodo inicializar_recursos_locales() {
    RecursosNodo recursos = malloc(sizeof(struct _RecursosNodo));
    assert(recursos != NULL);

    unsigned int cpus = obtener_cpus();
    recursos->cpu = inicializar_recurso(cpus);

    unsigned long mem = obtener_mem();
    recursos->mem = inicializar_recurso(mem);

    unsigned int gpu = obtener_gpus();
    recursos->gpu = inicializar_recurso(gpu);

    return recursos;
}

int reservar_recurso(RecursosNodo nodo, TablaJobs tablaJobs, unsigned long jobId, 
                     TipoRecurso rec, unsigned long cant, char ip[], unsigned short puerto) {
    Recurso recurso;

    if (rec == CPU) {
        recurso = nodo->cpu;
    } else if (rec == MEM) {
        recurso = nodo->mem;
    } else {
        recurso = nodo->gpu;
    }

    pthread_mutex_lock(&recurso->mutex);

    if (recurso->cantDisp >= cant) {
        recurso->cantDisp -= cant;

        pthread_mutex_unlock(&recurso->mutex);
        DatosJob *nuevosDatos = malloc(sizeof(DatosJob));
        assert(nuevosDatos != NULL);
        
        nuevosDatos->jobId = jobId;
        strncpy(nuevosDatos->nodoIp, ip, 16);
        nuevosDatos->nodoPuerto = puerto;
        
        // falta asignar recursos del job

        tablajobs_insertar(tablaJobs, nuevosDatos);
        return 1;
    } else {
        Solicitud nuevaSolicitud = malloc(sizeof(struct _Solicitud));
        assert(nuevaSolicitud != NULL);
        
        nuevaSolicitud->jobId = jobId;
        strncpy(nuevaSolicitud->ip, ip, 16);
        nuevaSolicitud->puerto = puerto;
        nuevaSolicitud->cant = cant;

        cola_encolar(recurso->solicitudPend, nuevaSolicitud);

        pthread_mutex_unlock(&recurso->mutex);
        return 0;
    }
}
        