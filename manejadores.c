#include "manejadores.h"
#include "estructuras/recursos.h"
#include "estructuras/tablajobs.h"
#include "estructuras/tablanodos.h"

ClienteConexion *crear_cliente(int clienteFD, int tipo, char ip[]) {
    ClienteConexion *nuevoCliente = malloc(sizeof(ClienteConexion));

    nuevoCliente->fd = clienteFD;

    nuevoCliente->tipo = tipo;

    strncpy(nuevoCliente->ip, ip, INET_ADDRSTRLEN);

    return nuevoCliente;
}

int leer_y_procesar_cliente(ClienteConexion *cliente, RecursosNodo recNodo,
                            TablaJobs tablaJobs, TablaNodos tablaNodos,
                            int erlangSchedulerSocket) {
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
                             tablaNodos, erlangSchedulerSocket);
        } else {
            manejar_cliente_erlang(cliente, cliente->buffer, tablaJobs,
                                   tablaNodos);
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
                      TablaNodos tablaNodos, int erlangSchedulerSocket) {
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

            DatosNodo datos = tablanodos_buscar(tablaNodos, cliente->ip);

            int estadoSolicitud = reservar_recurso(
                recNodo, tablaJobs, jobId, tipo_recurso_desde_string(recurso),
                cant, cliente->ip, datos.puerto, cliente->fd);

            if (estadoSolicitud == 1) {
                printf("[AGENTE C %d] El recurso se reservo correctamente\n",
                       cliente->fd);
                enviar_formateado(cliente->fd, "GRANTED %lu\n", jobId);
            }

            if (estadoSolicitud == 0) {
                printf(
                    "[AGENTE C % d] La reserva se agregó a la cola de espera\n",
                    cliente->fd);
            }

            if (estadoSolicitud == -1) {
                printf("[AGENTE C % d] No se pudo realizar la reserva\n",
                       cliente->fd);
                enviar_formateado(cliente->fd, "DENIED %lu\n", jobId);
            }
        }

    } else if (strcmp(comando, "GRANTED") == 0) {

        if (sscanf(mensaje, "GRANTED %lu", &jobId) == 1) {
            printf("[AGENTE C %d] Concedio GRANTED: Job=%lu\n", cliente->fd,
                   jobId);

            if (job_granted(tablaJobs, jobId)) {
                enviar_formateado(erlangSchedulerSocket, "JOB_GRANTED %lu\n",
                                  jobId);
            }
        }

    } else if (strcmp(comando, "DENIED") == 0) {

        if (sscanf(mensaje, "DENIED %lu", &jobId) == 1) {
            printf("[AGENTE C %d] Denego DENIED: Job=%lu\n", cliente->fd,
                   jobId);

            enviar_formateado(erlangSchedulerSocket, "JOB_DENIED %lu\n", jobId);
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

void eliminar_job(TablaJobs tablaJobs, unsigned long jobId) {
    int cantCpu = tablajobs_restar_recurso(tablaJobs, jobId, CPU);
}

void manejar_cliente_erlang(ClienteConexion *cliente, const char *mensaje,
                            TablaJobs tablaJobs, TablaNodos tablaNodos) {

    if (strncmp(mensaje, "JOB_REQUEST", 11) == 0) {
        unsigned long jobId;
        int offset = 0;

        if (sscanf(mensaje, "JOB_REQUEST %lu%n", &jobId, &offset) == 1) {
            printf("[ERLANG %d] Inicia Job ID: %lu\n", cliente->fd, jobId);

            // Colocamos un puntero justo donde terminó de leer el jobId
            const char *ptr = mensaje + offset;

            char host[16];
            char recurso[16];
            int cantidad;
            int bytes_leidos;

            // Desglose del formato " @%15[^:]:%15[^:]:%d%n":
            // [Espacio] : Ignora cualquier cantidad de espacios o tabs antes
            // del @
            // @         : Espera el símbolo literal @
            // %15[^:]   : Lee hasta 15 caracteres que NO sean ':' (lo guarda en
            // host)
            // :         : Espera los dos puntos literales
            // %15[^:]   : Lee hasta 15 caracteres que NO sean ':' (lo guarda en
            // recurso)
            // :         : Espera los dos puntos literales
            // %d        : Lee el entero (cantidad)
            // %n        : Guarda cuántos caracteres ocupó este bloque entero

            while (sscanf(ptr, " @%15[^:]:%15[^:]:%d%n", host, recurso,
                          &cantidad, &bytes_leidos) == 3) {
                printf("  -> Dependencia: Host=%s, Recurso=%s, Cantidad=%d\n",
                       host, recurso, cantidad);

                // TODO Lógica:
                // - Guardar en tu tabla del Job que requiere 'cantidad' de
                // 'recurso' en 'host'.
                // - Si host es tu propia IP (o 127.0.0.1), reservas localmente.
                // - Si es otra IP, lo encolas para enviar un RESERVE a ese
                // Agente C.

                // Avanzamos el puntero para la siguiente iteración del while
                ptr += bytes_leidos;
            }

            // Cuando el while termina, significa que ya procesamos todos los
            // recursos.
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

    // 2. Comando: GET NODES
    else if (strncmp(mensaje, "GET NODES", 9) == 0) {
        printf("[ERLANG %d] Solicito la lista de nodos activos descubiertos\n",
               cliente->fd);

        enviar_formateado(cliente->fd, "%s",
                          tablanodos_obtener_nodos(tablaNodos));

    }

    else if (strncmp(mensaje, "JOB_RELEASE", 12) == 0) {
        unsigned long jobId;
        if (sscanf(mensaje, "JOB_RELEASE %lu", &jobId) == 1) {
            printf("[ERLANG %d] Planificador notifica fin de Job ID: %lu\n",
                   cliente->fd, jobId);

            eliminar_job(tablaJobs, jobId);
            // TODO Lógica:
            // - Liberar los recursos locales asociados a este jobId.
            // - Enviar los comandos "RELEASE" correspondientes a los agentes C
            //   remotos que hayan prestado recursos para este trabajo.
        }
    }

    // else {
    //     fprintf(stderr, "[Erlang FD %d] Comando no reconocido: %s\n",
    //             cliente->fd, mensaje);
    //
    //     // Buena práctica: Enviar un mensaje de error legible al cliente para
    //     // evitar que se quede colgado
    //     char err_msg[] = "ERROR: Unknown command\n";
    //     send(cliente->fd, err_msg, strlen(err_msg), 0);
    // }
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

void manejar_timer(int timerSocket, int udp_sock, int puerto_udp) {
    // Vacío el timerfd para que epoll no siga notificando
    uint64_t exp;
    read(timerSocket, &exp, sizeof(exp));
    anuncio_broadcast(udp_sock, puerto_udp);
}

void anuncio_broadcast(int udp_sock, int puerto_udp) {
    char mensaje_anuncio[256];
    snprintf(mensaje_anuncio, sizeof(mensaje_anuncio),
             "ANNOUNCE 12000 cpu:4 mem:4096 gpu:0\n");

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(puerto_udp);
    dest_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    sendto(udp_sock, mensaje_anuncio, strlen(mensaje_anuncio), 0,
           (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    printf("Anuncio broadcast enviado\n");
}
