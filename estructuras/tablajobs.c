#include "tablajobs.h"

TablaJobs tablajobs_crear() {
	TablaJobs tabla = malloc(sizeof(struct _TablaJobs));
	assert(tabla != NULL);

    tabla->tablaPorId = calloc(MAX_JOBS, sizeof(JobActivo*));
	assert(tabla->tablaPorId != NULL);

    tabla->tablaPorNodo = calloc(MAX_NODOS, sizeof(JobActivo*));
	assert(tabla->tablaPorNodo != NULL);

	tabla->cantJobs = 0;
    pthread_mutex_init(&tabla->mutex, NULL);

	return tabla;
}

void tablajobs_destruir(TablaJobs tabla) {
	assert(tabla != NULL);

    for(int i = 0; i < MAX_JOBS; i++){
        JobActivo *actual = tabla->tablaPorId[i];
        while(actual != NULL){
            JobActivo *sig = actual->sigJobId;
            free(actual->datos);
            free(actual);
            actual = sig;
        }
    }

    free(tabla->tablaPorId);
    free(tabla->tablaPorNodo);

    pthread_mutex_destroy(&tabla->mutex);
	free(tabla);
}

void tablajobs_insertar(TablaJobs tabla, DatosJob *datos) {
    unsigned hashDatos = hash_ip_puerto(datos->nodoIp, datos->nodoPuerto);

    pthread_mutex_lock(&tabla->mutex);

	// Calculamos la posicion de acuerdo a su jobId.
    unsigned idx = datos->jobId % MAX_JOBS;

    // Buscamos si el dato ya existe para evitar duplicados
    JobActivo *actual = tabla->tablaPorId[idx];
    while (actual != NULL) {
        if (actual->datos->jobId == datos->jobId) {
            pthread_mutex_unlock(&tabla->mutex);
            return; // No hay que agregar nuevos jobs
        }
        actual = actual->sigJobId;
    }

    // Creamos el nodo
    JobActivo *nuevoJob = malloc(sizeof(JobActivo));
    assert(nuevoJob != NULL);
	nuevoJob->datos = datos;

    // Insertamos el job en la tabla de jobs por id
    nuevoJob->antJobId = NULL;
    nuevoJob->sigJobId = tabla->tablaPorId[idx];

    if (tabla->tablaPorId[idx] != NULL) {
        tabla->tablaPorId[idx]->antJobId = nuevoJob;
    }
    tabla->tablaPorId[idx] = nuevoJob;

    // Calculamos su posición de acuerdo al nodo asociado
    int nidx = hashDatos % MAX_NODOS;

    // Insertamos el job en la tabla de jobs por nodo
    nuevoJob->antJobNodo = NULL;
    nuevoJob->sigJobNodo = tabla->tablaPorNodo[nidx];

    if (tabla->tablaPorNodo[nidx] != NULL) {
        tabla->tablaPorNodo[nidx]->antJobNodo = nuevoJob;
    }
    tabla->tablaPorNodo[nidx] = nuevoJob;

    tabla->cantJobs++;

    pthread_mutex_unlock(&tabla->mutex);
}

void tablajobs_borrar_por_id(TablaJobs tabla, unsigned long jobId) {
    unsigned idx = jobId % MAX_JOBS;
    JobActivo *jobAEliminar = NULL;
    
    pthread_mutex_lock(&tabla->mutex);

    JobActivo *actual = tabla->tablaPorId[idx];
    while (actual != NULL) {
        JobActivo *sig = actual->sigJobId;
        if (actual->datos->jobId == jobId) {
            jobAEliminar = actual;
            desconectar_job(tabla, actual);

            tabla->cantJobs--;
            break;
        }
    }

    pthread_mutex_unlock(&tabla->mutex);

    if (jobAEliminar != NULL) {
        free(jobAEliminar->datos);
        free(jobAEliminar);
    }
}

void tablajobs_borrar_por_nodo(TablaJobs tabla, char ip[], unsigned short puerto) {
    unsigned idx = hash_ip_puerto(ip, puerto) % MAX_NODOS;
    JobActivo *jobsABorrar = NULL; 

    pthread_mutex_lock(&tabla->mutex);

    JobActivo *actual = tabla->tablaPorNodo[idx];
    while (actual != NULL) {
        JobActivo *sig = actual->sigJobId;

        if (strcmp(ip, actual->datos->nodoIp) == 0 && puerto == actual->datos->nodoPuerto) {
            desconectar_job(tabla, actual);
            tabla->cantJobs--;

            actual->sigJobId = jobsABorrar;
            jobsABorrar = actual;
        }
        actual = sig;
    }

    pthread_mutex_unlock(&tabla->mutex);
    
    while (jobsABorrar != NULL) {
        JobActivo *sig = jobsABorrar->sigJobId;
        free(jobsABorrar->datos);
        free(jobsABorrar);
        jobsABorrar = sig;
    }
}

static void desconectar_job(TablaJobs tabla, JobActivo* job) {
    // Reajustamos los punteros de colisión de la tabla de jobs por id
    if (job->antJobId == NULL) {
        unsigned idx = job->datos->jobId % MAX_JOBS;
        tabla->tablaPorId[idx] = job->sigJobId;
    } else {
        job->antJobId->sigJobId = job->sigJobId;
    }

    if (job->sigJobId != NULL) {
        job->sigJobId->antJobId = job->antJobId;
    }

    // Reajustamos los punteros de colisión de la tabla de jobs por nodo
    if (job->antJobNodo == NULL) {
        unsigned nidx = hash(job->datos) % MAX_NODOS;
        tabla->tablaPorNodo[nidx] = job->sigJobNodo;
    } else {
        job->antJobNodo->sigJobNodo = job->sigJobNodo;
    }

    if (job->sigJobNodo != NULL) {
        job->sigJobNodo->antJobNodo = job->antJobNodo;
    }
}