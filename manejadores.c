#include "manejadores.h"

#define debug(str) fprintf(stderr, "HOLA: %s\n", str);

int enviar_formateado(int fd, const char *formato, ...);

ClienteConexion *crear_cliente(int clienteFD, int tipo, char ip[],
                               char *mensaje) {
    ClienteConexion *nuevoCliente = malloc(sizeof(ClienteConexion));

    nuevoCliente->fd = clienteFD;

    nuevoCliente->tipo = tipo;

    if (ip != NULL)
        strncpy(nuevoCliente->ip, ip, INET_ADDRSTRLEN);

    if (mensaje != NULL)
        strncpy(nuevoCliente->mensaje, mensaje, 256);

    return nuevoCliente;
}

int leer_y_procesar_cliente(ClienteConexion *cliente, RecursosNodo recNodo,
                            TablaJobs tablaJobs, TablaNodos tablaNodos,
                            int erlangFd, int epollFd) {

    if (cliente->tipo == CONEXION_SALIENTE) {
        debug("10");

        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(cliente->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
            perror("getsockopt falló");
            return 1;
        }

        if (error != 0) {
            fprintf(stderr,
                    "[WARN] Fallo al conectar con la IP %s. Razón: %s\n",
                    cliente->ip, strerror(error));
            return 1;
        }

        enviar_formateado(cliente->fd, cliente->mensaje);

        cliente->tipo = CLIENTE_AGENTE_C;

        return 0;
    }

    int espacio_libre = sizeof(cliente->buffer) - cliente->bytes_in_buffer - 1;

    int bytes_leidos = read(
        cliente->fd, cliente->buffer + cliente->bytes_in_buffer, espacio_libre);

    if (bytes_leidos == 0) {
        printf("[%s %d] Desconexion\n",
               cliente->tipo == CLIENTE_AGENTE_C ? "AGENTE C" : "ERLANG",
               cliente->fd);
        return 1;
    }

    else if (bytes_leidos == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        perror("read error");
        return 1;
    }

    cliente->bytes_in_buffer += bytes_leidos;
    cliente->buffer[cliente->bytes_in_buffer] = '\0';

    char *newline_ptr;
    while ((newline_ptr = strchr(cliente->buffer, '\n')) != NULL) {

        *newline_ptr = '\0';
        int longitud_mensaje = newline_ptr - cliente->buffer;

        if (cliente->tipo == CLIENTE_AGENTE_C) {
            manejar_agente_c(cliente, cliente->buffer, recNodo, tablaJobs,
                             tablaNodos, erlangFd);
        } else {
            manejar_cliente_erlang(cliente, cliente->buffer, tablaJobs,
                                   tablaNodos, epollFd, recNodo);
        }

        int bytes_sobrantes = cliente->bytes_in_buffer - (longitud_mensaje + 1);

        if (bytes_sobrantes > 0) {
            // Comportamiento de cola
            memmove(cliente->buffer, newline_ptr + 1, bytes_sobrantes);
        }

        cliente->bytes_in_buffer = bytes_sobrantes;

        cliente->buffer[cliente->bytes_in_buffer] = '\0';
    }

    return 0;
}

int tipo_recurso_desde_string(char *s) {
    if (strcmp("cpu", s) == 0)
        return 0;
    if (strcmp("mem", s) == 0)
        return 1;
    if (strcmp("gpu", s) == 0)
        return 2;
    return -1;
}

int enviar_formateado(int fd, const char *formato, ...) {
    char buffer[MAX_MSG_LEN];

    va_list args;
    va_start(args, formato);

    int longitud = vsnprintf(buffer, sizeof(buffer), formato, args);
    va_end(args);

    if (longitud < 0 || longitud >= MAX_MSG_LEN) {
        fprintf(stderr, "[ERROR] Mensaje demasiado largo o error de formato\n");
        return -1;
    }

    int bytes_enviados = send(fd, buffer, longitud, 0);
    if (bytes_enviados == -1) {
        perror("Error en send formateado");
        return -1;
    }

    return 0;
}

void notificar_lista_promovidos(ListaPromovidos lista) {
    struct _NodoPromovido *actual = lista;

    while (actual != NULL) {
        printf("[PROMOVIDOS] Notificando a FD %d (Job %lu)\n", actual->fd,
               actual->jobId);

        if (enviar_formateado(actual->fd, "GRANTED %lu\n", actual->jobId) ==
            0) {
            printf("[PROMOVIDOS] Notificación exitosa\n");
        }

        actual = actual->sig;
    }
}

void manejar_agente_c(ClienteConexion *cliente, const char *mensaje,
                      RecursosNodo recNodo, TablaJobs tablaJobs,
                      TablaNodos tablaNodos, int erlangFd) {
    char comando[16];
    unsigned long jobId;
    char recurso[16];
    unsigned long cant;

    if (sscanf(mensaje, "%15s", comando) != 1) {
        fprintf(stderr, "[AGENTE C %d] Linea vacia o mal formada\n",
                cliente->fd);
        return;
    }

    if (strcmp(comando, "RESERVE") == 0) {

        if (sscanf(mensaje, "RESERVE %lu %15s %lu", &jobId, recurso, &cant) ==
            3) {
            printf("[AGENTE C %d] Solicita RESERVE: Job=%lu, Recurso=%s, "
                   "Cantidad=%lu\n",
                   cliente->fd, jobId, recurso, cant);

            DatosNodo *datos = tablanodos_buscar(tablaNodos, cliente->ip);
            if (datos != NULL) {

                int estadoSolicitud =
                    reservar_recurso(recNodo, tablaJobs, jobId,
                                     tipo_recurso_desde_string(recurso), cant,
                                     cliente->ip, datos->puerto, cliente);

                if (estadoSolicitud == 1) {
                    printf(
                        "[AGENTE C %d] El recurso se reservo correctamente\n",
                        cliente->fd);
                    enviar_formateado(cliente->fd, "GRANTED %lu\n", jobId);
                }

                if (estadoSolicitud == 0) {
                    printf("[AGENTE C % d] La reserva se agregó a la cola de "
                           "espera\n",
                           cliente->fd);
                }

                if (estadoSolicitud == -1) {
                    printf("[AGENTE C % d] No se pudo realizar la reserva\n",
                           cliente->fd);
                    enviar_formateado(cliente->fd, "DENIED %lu\n", jobId);
                }
            }
        }

    } else if (strcmp(comando, "GRANTED") == 0) {

        if (sscanf(mensaje, "GRANTED %lu", &jobId) == 1) {
            printf("[AGENTE C %d] Concedio GRANTED: Job=%lu\n", cliente->fd,
                   jobId);

            DatosNodo *datos = tablanodos_buscar(tablaNodos, cliente->ip);
            tablajobs_recurso_granted(tablaJobs, jobId, cliente->ip,
                                      datos->puerto);

            if (tablajobs_job_granted(tablaJobs, jobId)) {
                enviar_formateado(erlangFd, "JOB_GRANTED %lu\n", jobId);
            }
        }

    } else if (strcmp(comando, "DENIED") == 0) {

        if (sscanf(mensaje, "DENIED %lu", &jobId) == 1) {
            printf("[AGENTE C %d] Denego DENIED: Job=%lu\n", cliente->fd,
                   jobId);

            enviar_formateado(erlangFd, "JOB_DENIED %lu\n", jobId);
        }

    } else if (strcmp(comando, "RELEASE") == 0) {

        if (sscanf(mensaje, "RELEASE %lu %15s %lu", &jobId, recurso, &cant) ==
            3) {
            printf("[AGENTE C %d] Notifica RELEASE: Job=%lu, Recurso=%s, "
                   "Cantidad=%lu\n",
                   cliente->fd, jobId, recurso, cant);

            ListaPromovidos lista = liberar_recurso(
                recNodo, tablaJobs, jobId, tipo_recurso_desde_string(recurso));

            notificar_lista_promovidos(lista);
        }

    } else {
        fprintf(stderr, "[AGENTE C %d] Comando desconocido de Agente C: %s\n",
                cliente->fd, comando);
    }
}

void liberar_job(TablaJobs tablaJobs, unsigned long jobId) {
    debug("ENTRO");
    ListaResultados lista = tablajobs_release_job(tablaJobs, jobId);
    printf("SALGO\n");
    debug("SALGO");
    struct _NodoResultado *actual = lista;

    while (actual != NULL) {
        char rec[4];
        int cant;
        if (actual->datos->recReservados->cpu > 0) {
            strcpy(rec, "cpu");
            cant = actual->datos->recReservados->cpu;
        } else if (actual->datos->recReservados->mem > 0) {
            strcpy(rec, "mem");
            cant = actual->datos->recReservados->mem;
        } else if (actual->datos->recReservados->gpu > 0) {
            strcpy(rec, "gpu");
            cant = actual->datos->recReservados->gpu;
        } else {
            continue;
        }
        debug("LIB");
        int fd = ((ClienteConexion *)actual->datos->datosCliente)->fd;
        enviar_formateado(fd, "RELEASE %lu %s %d\n", jobId, rec, cant);

        close(fd);
        free(actual->datos->datosCliente);

        actual = actual->sig;
    }
    debug("LIB1");
}

void manejar_cliente_erlang(ClienteConexion *cliente, const char *mensaje,
                            TablaJobs tablaJobs, TablaNodos tablaNodos,
                            int epollFd, RecursosNodo recNodo) {

    if (strncmp(mensaje, "JOB_REQUEST", 11) == 0) {
        unsigned long jobId;
        int offset = 0;

        if (sscanf(mensaje, "JOB_REQUEST %lu%n", &jobId, &offset) == 1) {
            printf("[ERLANG %d] Inicia Job ID: %lu\n", cliente->fd, jobId);

            // Colocamos un puntero justo donde terminó de leer el jobId
            const char *ptr = mensaje + offset;

            char ip[16];
            char recurso[16];
            int cantidad;
            int bytes_leidos;

            while (sscanf(ptr, " @%15[^:]:%15[^:]:%d%n", ip, recurso, &cantidad,
                          &bytes_leidos) == 3) {
                printf("  -> Dependencia: Host=%s, Recurso=%s, Cantidad=%d\n",
                       ip, recurso, cantidad);

                DatosNodo *datos = tablanodos_buscar(tablaNodos, ip);
                debug(ip);
                printf("puerto: %d", datos->puerto);
                int fdSalida =
                    crear_socket_saliente_nobloqueante(ip, datos->puerto);

                ClienteConexion *nueva_conexion =
                    crear_cliente(fdSalida, CONEXION_SALIENTE, ip, NULL);

                registrar_solicitud_propia(
                    tablaJobs, jobId, tipo_recurso_desde_string(recurso),
                    cantidad, ip, datos->puerto, nueva_conexion);

                snprintf(nueva_conexion->mensaje, 256, "RESERVE %lu %s %d\n",
                         jobId, recurso, cantidad);

                // Cuando se arme la conexion se enviara el mensaje guardado en
                // la estructura
                agregar_socket_epoll(epollFd, fdSalida, EPOLLOUT | EPOLLONESHOT,
                                     nueva_conexion, EPOLL_CTL_ADD);

                ptr += bytes_leidos;
            }

            printf("[ERLANG %d] Finalizado el parseo de dependencias para "
                   "Job %lu\n",
                   cliente->fd, jobId);

        } else {
            fprintf(stderr,
                    "[ERLANG %d] Error de sintaxis en el encabezado del "
                    "JOB_REQUEST\n",
                    cliente->fd);
        }
    }

    else if (strncmp(mensaje, "GET_NODES", 9) == 0) {
        printf("[ERLANG %d] Solicito la lista de nodos activos descubiertos\n",
               cliente->fd);
        tablanodos_borrar_expirados(tablaNodos, tablaJobs, recNodo);
        char *nodos = tablanodos_obtener_nodos(tablaNodos);
        enviar_formateado(cliente->fd, "NODES %s", nodos);
    }

    else if (strncmp(mensaje, "JOB_RELEASE", 11) == 0) {
        unsigned long jobId;
        if (sscanf(mensaje, "JOB_RELEASE %lu", &jobId) == 1) {
            printf("[ERLANG %d] Planificador notifica fin de Job ID: %lu\n",
                   cliente->fd, jobId);

            liberar_job(tablaJobs, jobId);
        }
    }
}

void registrar_nodo(int udp_sock, TablaNodos tablaNodos) {
    char buffer_udp[512];
    struct sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);

    int bytes_recibidos =
        recvfrom(udp_sock, buffer_udp, sizeof(buffer_udp) - 1, 0,
                 (struct sockaddr *)&sender_addr, &sender_len);

    if (bytes_recibidos > 0) {
        buffer_udp[bytes_recibidos] = '\0';

        if (strncmp(buffer_udp, "ANNOUNCE", 8) == 0) {

            DatosNodo *datos = malloc(sizeof(DatosNodo));

            if (inet_ntop(AF_INET, &(sender_addr.sin_addr), datos->ip,
                          INET_ADDRSTRLEN) == NULL) {
                perror("inet_ntop fallo al extraer la IP");
                return;
            }

            if (sscanf(buffer_udp, "ANNOUNCE %hu %63[^\n]", &datos->puerto,
                       datos->recursos) == 2) {

                printf("Descubierto nodo activo: IP=%s, Puerto=%hu, "
                       "Recursos=%s\n",
                       datos->ip, datos->puerto, datos->recursos);

                tablanodos_insertar(tablaNodos, datos);
            }
        }
    }
}

void manejar_timer(int timerSocket, int udp_sock, int puerto_udp,
                   RecursosNodo recNodo, int puertoTcpEscucha) {
    // Vacio el timerfd para que epoll no siga notificando
    uint64_t exp;
    read(timerSocket, &exp, sizeof(exp));
    anuncio_broadcast(udp_sock, puerto_udp, recNodo, puertoTcpEscucha);
}

void anuncio_broadcast(int udp_sock, int puerto_udp, RecursosNodo recNodo,
                       int puertoTcpEscucha) {
    char mensaje_anuncio[256];
    snprintf(mensaje_anuncio, sizeof(mensaje_anuncio),
             "ANNOUNCE %d cpu:%lu mem:%lu gpu:%lu\n", puertoTcpEscucha,
             recNodo->cpu->cantDisp, recNodo->mem->cantDisp,
             recNodo->gpu->cantDisp);

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(puerto_udp);
    dest_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    sendto(udp_sock, mensaje_anuncio, strlen(mensaje_anuncio), 0,
           (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    printf("Anuncio broadcast enviado\n");
}
