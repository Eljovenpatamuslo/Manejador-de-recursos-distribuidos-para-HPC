#include "manejadores.h"

ClienteConexion *crear_cliente(int eventfd, int tipo) {
    ClienteConexion *nuevo_cliente = malloc(sizeof(ClienteConexion));

    nuevo_cliente->fd = eventfd;

    nuevo_cliente->tipo = tipo;

    return nuevo_cliente;
}

int leer_y_procesar_cliente(ClienteConexion *cliente) {
    int espacio_libre = sizeof(cliente->buffer) - cliente->bytes_in_buffer - 1;

    int bytes_leidos = read(
        cliente->fd, cliente->buffer + cliente->bytes_in_buffer, espacio_libre);

    if (bytes_leidos == 0) {
        printf("El cliente FD %d se ha desconectado.\n", cliente->fd);
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
            manejar_agente_c(cliente, cliente->buffer);
        } else {
            manejar_cliente_erlang(cliente, cliente->buffer);
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

void manejar_agente_c(ClienteConexion *cliente, const char *mensaje) {
    char comando[16];
    int job_id;
    char recurso[16];
    int cantidad;

    if (sscanf(mensaje, "%15s", comando) != 1) {
        fprintf(stderr, "Recibida línea vacía o mal formada del fd %d\n",
                cliente->fd);
        return;
    }

    if (strcmp(comando, "RESERVE") == 0) {

        if (sscanf(mensaje, "RESERVE %d %15s %d", &job_id, recurso,
                   &cantidad) == 3) {
            printf("[FD %d] Solicita RESERVE: Job=%d, Recurso=%s, "
                   "Cantidad=%d\n",
                   cliente->fd, job_id, recurso, cantidad);

            // TODO Lógica:
            // - Buscar el recurso en tus variables locales.
            // - Si hay disponible:
            //      restar cantidad, guardar en tu tabla de jobs activos,
            //      hacer send(cliente->fd, "GRANTED <job_id>\n", ...).
            // - Si NO hay disponible:
            //      agregar job_id y cliente->fd a la cola FIFO del recurso.
        }

    } else if (strcmp(comando, "GRANTED") == 0) {

        if (sscanf(mensaje, "GRANTED %d", &job_id) == 1) {
            printf("[FD %d] Concedió GRANTED: Job=%d\n", cliente->fd, job_id);

            // TODO Lógica:
            // - Tú pediste un recurso y te lo dieron.
            // - Debes notificar a tu planificador Erlang local que esta
            //   parte del job fue exitosa.
        }

    } else if (strcmp(comando, "DENIED") == 0) {

        if (sscanf(mensaje, "DENIED %d", &job_id) == 1) {
            printf("[FD %d] Denegó DENIED: Job=%d\n", cliente->fd, job_id);

            // TODO Lógica:
            // - Tu petición fue rechazada. Avisar a Erlang.
        }

    } else if (strcmp(comando, "RELEASE") == 0) {

        if (sscanf(mensaje, "RELEASE %d %15s %d", &job_id, recurso,
                   &cantidad) == 3) {
            printf("[FD %d] Notifica RELEASE: Job=%d, Recurso=%s, "
                   "Cantidad=%d\n",
                   cliente->fd, job_id, recurso, cantidad);

            // TODO Lógica:
            // - Sumar la cantidad devuelta a tu recurso local.
            // - Revisar la cola FIFO de ese recurso: ¿alcanza para el
            // primer encolado?
            // - Si alcanza: descontar, enviarle GRANTED a ese socket
            // encolado.
        }

    } else {
        fprintf(stderr, "Comando desconocido de Agente C: %s\n", comando);
    }
}

void manejar_cliente_erlang(ClienteConexion *cliente, const char *mensaje) {

    if (strncmp(mensaje, "JOB_REQUEST", 11) == 0) {
        int job_id;
        int offset = 0;

        if (sscanf(mensaje, "JOB_REQUEST %d%n", &job_id, &offset) == 1) {
            printf("[Erlang FD %d] Inicia Job ID: %d\n", cliente->fd, job_id);

            // Colocamos un puntero justo donde terminó de leer el job_id
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
            printf("[Erlang FD %d] Finalizado el parseo de dependencias para "
                   "Job %d.\n",
                   cliente->fd, job_id);

        } else {
            fprintf(stderr, "[Erlang] Error de sintaxis en el encabezado del "
                            "JOB_REQUEST\n");
        }
    }

    // 2. Comando: GET NODES
    else if (strncmp(mensaje, "GET NODES", 9) == 0) {
        printf(
            "[Erlang FD %d] Solicitó la lista de nodos activos descubiertos\n",
            cliente->fd);

        // TODO Lógica:
        // - Recorrer tu tabla de nodos activos (la que actualiza el socket
        // UDP).
        // - Construir un string con la respuesta formateada según lo acordado
        // con Erlang.
        // - Ejemplo simbólico de cómo responder a través del socket:

    }

    // 3. Comando: JOB_CONCLUDE <job_id> (O el comando que defina la
    // finalización)
    else if (strncmp(mensaje, "JOB_CONCLUDE", 12) == 0) {
        int job_id;
        if (sscanf(mensaje, "JOB_CONCLUDE %d", &job_id) == 1) {
            printf("[Erlang FD %d] Planificador notifica fin de Job ID: %d\n",
                   cliente->fd, job_id);

            // TODO Lógica:
            // - Liberar los recursos locales asociados a este job_id.
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

void registrar_nodo(int udp_sock) {
    char buffer_udp[512];
    struct sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);

    int bytes_recibidos =
        recvfrom(udp_sock, buffer_udp, sizeof(buffer_udp) - 1, 0,
                 (struct sockaddr *)&sender_addr, &sender_len);

    if (bytes_recibidos > 0) {
        buffer_udp[bytes_recibidos] = '\0';

        if (strncmp(buffer_udp, "ANNOUNCE", 8) == 0) {
            // INET_ADDRSTRLEN es una constante estándar de C para el tamaño
            // máximo de una IPv4 (16)
            char ip[INET_ADDRSTRLEN];
            char recursos[64];
            int puerto;

            if (inet_ntop(AF_INET, &(sender_addr.sin_addr), ip,
                          INET_ADDRSTRLEN) == NULL) {
                perror("inet_ntop falló al extraer la IP");
                return;
            }

            if (sscanf(buffer_udp, "ANNOUNCE %d %63[^\n]", &puerto, recursos) ==
                2) {

                printf("Descubierto nodo activo: IP=%s, Puerto=%d, "
                       "Recursos=%s\n",
                       ip, puerto, recursos);

                // TODO: Aquí actualizas tu tabla de nodos en
                // memoria, registrando la IP, el puerto y el
                // timestamp actual con time(NULL) para luego
                // limpiar los caídos.
            }
        }
    }
}

void manejar_timer(int timerSocket, int udp_sock, int puerto_udp) {
    // Vacío el timerfd para que epoll no siga notificando
    uint64_t exp;
    read(timerSocket, &exp, sizeof(exp));

    char mensaje_anuncio[256];
    snprintf(mensaje_anuncio, sizeof(mensaje_anuncio),
             "ANNOUNCE puerto recursos\n");

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(puerto_udp);
    dest_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    sendto(udp_sock, mensaje_anuncio, strlen(mensaje_anuncio), 0,
           (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    printf("Anuncio broadcast enviado.\n");
}
