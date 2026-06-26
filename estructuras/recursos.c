#include "recursos.h"
#include "conexion.h"
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

int reservar_recurso(RecursosNodo nodo, TablaJobs tablaJobs,
                     unsigned long jobId, TipoRecurso rec, unsigned long cant,
                     char ip[], unsigned short puerto, void *datosCliente) {
    Recurso recurso;

    if (rec == CPU) {
        recurso = nodo->cpu;
    } else if (rec == MEM) {
        recurso = nodo->mem;
    } else {
        recurso = nodo->gpu;
    }

    pthread_mutex_lock(&recurso->mutex);

    if (cola_es_vacia(recurso->solicitudPend) && recurso->cantDisp >= cant) {
        recurso->cantDisp -= cant;

        DatosJob *nuevosDatos = malloc(sizeof(DatosJob));
        assert(nuevosDatos != NULL);

        nuevosDatos->jobId = jobId;
        nuevosDatos->rol = JOB_REMOTO;
        nuevosDatos->datosCliente = datosCliente;
        strncpy(nuevosDatos->nodoIp, ip, 16);
        nuevosDatos->nodoPuerto = puerto;
        nuevosDatos->recReservados = inicializar_recursos_reservados(rec, cant);
        nuevosDatos->recPedidos = inicializar_recursos_reservados(rec, cant);

        tablajobs_insertar(tablaJobs, nuevosDatos);

        pthread_mutex_unlock(&recurso->mutex);
        return 1; // El recurso se reservo correctamente GRANTED

    } else if (recurso->capacidad >= cant) {

        Solicitud nuevaSolicitud = malloc(sizeof(struct _Solicitud));
        assert(nuevaSolicitud != NULL);

        nuevaSolicitud->jobId = jobId;
        strncpy(nuevaSolicitud->ip, ip, 16);
        nuevaSolicitud->puerto = puerto;
        nuevaSolicitud->cant = cant;
        nuevaSolicitud->datosCliente = datosCliente;

        cola_encolar(recurso->solicitudPend, nuevaSolicitud);

        pthread_mutex_unlock(&recurso->mutex);
        return 0; // La reserva se agrego a la cola de espera
    }

    return -1; // La reserva supera la capacidad máxima del recurso DENIED
}

ListaPromovidos liberar_recurso(RecursosNodo nodo, TablaJobs tablaJobs,
                                unsigned long jobId, TipoRecurso rec) {
    Recurso recurso;
    if (rec == CPU) recurso = nodo->cpu;
    else if (rec == MEM) recurso = nodo->mem;
    else recurso = nodo->gpu;

    unsigned long cantALiberar = tablajobs_restar_recurso(tablaJobs, jobId, rec);
    if (cantALiberar == 0) return NULL;

    pthread_mutex_lock(&recurso->mutex);
    recurso->cantDisp += cantALiberar;

    ListaPromovidos promovidosPrimero = NULL;

    while (!cola_es_vacia(recurso->solicitudPend)) {
        Solicitud solPendiente = (Solicitud)cola_inicio(recurso->solicitudPend);
        if (recurso->cantDisp >= solPendiente->cant) {
            cola_desencolar(recurso->solicitudPend);
            recurso->cantDisp -= solPendiente->cant;

            unsigned long solJobId = solPendiente->jobId;
            unsigned long solCant = solPendiente->cant;
            void *solDatosCliente = solPendiente->datosCliente;
            char solIp[16];
            strncpy(solIp, solPendiente->ip, 16);
            unsigned short solPuerto = solPendiente->puerto;
            free(solPendiente);

            DatosJob *nuevosDatos = malloc(sizeof(DatosJob));
            assert(nuevosDatos != NULL);
            nuevosDatos->jobId = solJobId;
            nuevosDatos->rol = JOB_REMOTO;
            nuevosDatos->datosCliente = solDatosCliente;
            strncpy(nuevosDatos->nodoIp, solIp, 16);
            nuevosDatos->nodoPuerto = solPuerto;
            nuevosDatos->recReservados = inicializar_recursos_reservados(rec, solCant);
            nuevosDatos->recPedidos = inicializar_recursos_reservados(rec, solCant);
            tablajobs_insertar(tablaJobs, nuevosDatos);

            // Nodo promovido: almacena directamente el puntero al ClienteConexion
            struct _NodoPromovido *nuevoNodo = malloc(sizeof(struct _NodoPromovido));
            assert(nuevoNodo != NULL);
            nuevoNodo->jobId = solJobId;
            nuevoNodo->cliente = (ClienteConexion *)solDatosCliente;
            nuevoNodo->sig = promovidosPrimero;
            promovidosPrimero = nuevoNodo;
        } else {
            break;
        }
    }

    pthread_mutex_unlock(&recurso->mutex);
    return promovidosPrimero;
}

void liberar_recursos_reservados(RecursosNodo recursos,
                                 RecursosReservados reservados) {
    pthread_mutex_lock(&recursos->cpu->mutex);
    recursos->cpu->cantDisp += reservados->cpu;
    pthread_mutex_unlock(&recursos->cpu->mutex);

    pthread_mutex_lock(&recursos->mem->mutex);
    recursos->mem->cantDisp += reservados->mem;
    pthread_mutex_unlock(&recursos->mem->mutex);

    pthread_mutex_lock(&recursos->gpu->mutex);
    recursos->gpu->cantDisp += reservados->gpu;
    pthread_mutex_unlock(&recursos->gpu->mutex);
}

RecursosReservados inicializar_recursos_reservados(TipoRecurso rec,
                                                   unsigned long cant) {
    RecursosReservados res = malloc(sizeof(struct _RecursosReservados));
    res->cpu = rec == CPU ? cant : 0;
    res->mem = rec == MEM ? cant : 0;
    res->gpu = rec == GPU ? cant : 0;

    return res;
}

int comp_recursos(RecursosReservados a, RecursosReservados b) {
    return a->cpu == b->cpu && a->mem == b->mem && a->gpu == b->gpu;
}

