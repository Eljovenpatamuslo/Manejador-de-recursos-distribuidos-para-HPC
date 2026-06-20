#define _GNU_SOURCE
#include "sockets.h"
#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>

#define PUBLIC_DIR "127.0.0.2"
#define PRIVATE_DIR "127.0.0.2"
#define PUERTO_TCP 8000
#define PUERTO_UDP 8100
#define MAX_EV_EPOLL 64

int public_listen_sock;
int local_listen_sock;
int udp_sock;
int epollfd;
int timer_sock;

typedef enum { CLIENTE_ERLANG, CLIENTE_AGENTE_C } TipoCliente;

typedef struct {
    int fd;
    TipoCliente tipo;
    char buffer[4096];
    int bytes_in_buffer;
} ClienteConexion;

static void manejar_conn_sock_epoll(ClienteConexion *cliente);
static int aceptar_listen_sock_epoll(int eventfd);
static void anuncio_broadcast();
static int leer_y_procesar_cliente(ClienteConexion *cliente);
static void rearmar_fd_epoll(int eventfd);
static void registrar_nodo();

void *gestionar_epoll(void *arg) {
    struct epoll_event events[MAX_EV_EPOLL];
    struct epoll_event ev;

    for (int num_fds, n;;) {
        num_fds = epoll_wait(epollfd, events, MAX_EV_EPOLL, -1);
        if (num_fds == -1)
            perror("epoll_wait");

        for (n = 0; n < num_fds; ++n) {
            int eventfd = events[n].data.fd;

            if (eventfd == timer_sock) {
                // Debo anunciarme
                anuncio_broadcast();
                rearmar_fd_epoll(eventfd);
            }

            else if (eventfd == udp_sock) {
                // Anuncio de otro nodo
                registrar_nodo();
                rearmar_fd_epoll(eventfd);
            }

            else if (eventfd == local_listen_sock ||
                     eventfd == public_listen_sock) {
                if (aceptar_listen_sock_epoll(eventfd) == 1)
                    continue;
            }

            else {
                ClienteConexion *cliente =
                    (ClienteConexion *)events[n].data.ptr;

                int sock_closed = leer_y_procesar_cliente(cliente);

                if (sock_closed) {
                    close(cliente->fd);
                    free(cliente);
                } else {
                    ev.events = EPOLLIN | EPOLLONESHOT;
                    ev.data.ptr = cliente;

                    if (epoll_ctl(epollfd, EPOLL_CTL_MOD, cliente->fd, &ev) ==
                        -1) {
                        perror("epoll_ctl: MOD");
                        close(cliente->fd);
                        free(cliente);
                    }
                }
            }
        }
    }
}

void rearmar_fd_epoll(int eventfd) {
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLONESHOT;
    ev.data.fd = eventfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_MOD, eventfd, &ev) == -1) {
        perror("epoll_ctl: MOD");
        close(eventfd);
    }
}

static int aceptar_listen_sock_epoll(int eventfd) {
    struct epoll_event ev;
    int conn_sock = accept4(eventfd, NULL, NULL, SOCK_NONBLOCK);

    if (conn_sock == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 1;
        }
        perror("accept4");
        exit(EXIT_FAILURE);
    }

    ClienteConexion *nuevo_cliente = malloc(sizeof(ClienteConexion));

    nuevo_cliente->fd = eventfd;

    if (eventfd == public_listen_sock)
        nuevo_cliente->tipo = CLIENTE_AGENTE_C;
    else
        nuevo_cliente->tipo = CLIENTE_ERLANG;

    ev.events = EPOLLIN | EPOLLONESHOT;
    ev.data.ptr = nuevo_cliente;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev) == -1) {
        perror("epoll_ctl: conn_sock");
        exit(EXIT_FAILURE);
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

int leer_y_procesar_cliente(ClienteConexion *cliente) {
    int espacio_libre = sizeof(cliente->buffer) - cliente->bytes_in_buffer - 1;

    int bytes_leidos = read(
        cliente->fd, cliente->buffer + cliente->bytes_in_buffer, espacio_libre);

    if (bytes_leidos == 0) {
        printf("El cliente FD %d se ha desconectado.\n", cliente->fd);
        // Liberar recursos
        return 1;
    } else if (bytes_leidos == -1) {
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
            manejar_erlang(cliente, cliente->buffer);
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

int crear_timer_anuncio(int intervalo_segundos) {
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (tfd == -1) {
        perror("timerfd_create");
        return -1;
    }

    struct itimerspec ts;
    // Primera vez que se dispara
    ts.it_value.tv_sec = intervalo_segundos;
    ts.it_value.tv_nsec = 0;
    // Intervalo de repetición
    ts.it_interval.tv_sec = intervalo_segundos;
    ts.it_interval.tv_nsec = 0;

    if (timerfd_settime(tfd, 0, &ts, NULL) == -1) {
        perror("timerfd_settime");
        close(tfd);
        return -1;
    }

    return tfd;
}

void anuncio_broadcast() {
    char mensaje_anuncio[256];
    snprintf(mensaje_anuncio, sizeof(mensaje_anuncio),
             "ANNOUNCE puerto recursos\n");

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PUERTO_UDP);
    dest_addr.sin_addr.s_addr =
        inet_addr("255.255.255.255"); // IP de Broadcast universal

    sendto(udp_sock, mensaje_anuncio, strlen(mensaje_anuncio), 0,
           (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    printf("Anuncio broadcast enviado.\n");
}

void registrar_nodo() {
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

void agregar_socket_epoll(int socket, int flags) {
    struct epoll_event ev;
    ev.events = flags;
    ev.data.fd = socket;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, socket, &ev) == -1) {
        perror("epoll_ctl");
        exit(EXIT_FAILURE);
    }
}

int main() {
    public_listen_sock = crear_nonblocking_socket(PUBLIC_DIR, PUERTO_TCP);
    local_listen_sock = crear_nonblocking_socket(PUBLIC_DIR, PUERTO_TCP);
    udp_sock = crear_socket_udp_broadcast(PUERTO_UDP);
    timer_sock = crear_timer_anuncio(5);

    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("epoll_create1");
    }

    agregar_socket_epoll(public_listen_sock, EPOLLIN);
    agregar_socket_epoll(local_listen_sock, EPOLLIN);
    agregar_socket_epoll(udp_sock, EPOLLIN | EPOLLONESHOT);
    agregar_socket_epoll(timer_sock, EPOLLIN | EPOLLONESHOT);

    int nproc = cant_nucleos();

    pthread_t id[nproc];

    for (int i = 0; i < nproc; i++) {
        pthread_create(&id[i], NULL, gestionar_epoll, NULL);
    }

    pthread_join(id[0], NULL);

    return 0;
}
