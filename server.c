#define _GNU_SOURCE
#include "manejadores.h"
#include "sockets.h"
#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/epoll.h>

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

static void agregar_socket_epoll(int socket, int evFlags, void *evDataPtr,
                                 int epollCtlFlags);
static void manejar_conn_sock_epoll(ClienteConexion *cliente);
static int aceptar_listen_sock_epoll(int eventfd);
static void rearmar_fd_epoll(int eventfd);

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

    ClienteConexion *nuevo_cliente =
        crear_cliente(eventfd, eventfd == public_listen_sock ? CLIENTE_AGENTE_C
                                                             : CLIENTE_ERLANG);
    agregar_socket_epoll(conn_sock, EPOLLIN | EPOLLONESHOT, nuevo_cliente,
                         EPOLL_CTL_MOD);

    return 0;
}

void agregar_socket_epoll(int socket, int evFlags, void *evDataPtr,
                          int epollCtlFlags) {
    struct epoll_event ev;
    ev.events = evFlags;
    ev.data.fd = socket;
    ev.data.ptr = evDataPtr;

    if (epoll_ctl(epollfd, epollCtlFlags, socket, &ev) == -1) {
        perror("epoll_ctl");
        free(evDataPtr);
        exit(EXIT_FAILURE);
    }
}

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
                anuncio_broadcast(udp_sock, PUERTO_UDP);
                agregar_socket_epoll(eventfd, EPOLLIN | EPOLLONESHOT, NULL,
                                     EPOLL_CTL_MOD);
            }

            else if (eventfd == udp_sock) {
                // Anuncio de otro nodo
                registrar_nodo(udp_sock);
                agregar_socket_epoll(eventfd, EPOLLIN | EPOLLONESHOT, NULL,
                                     EPOLL_CTL_MOD);
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
                    agregar_socket_epoll(cliente->fd, EPOLLIN | EPOLLONESHOT,
                                         cliente, EPOLL_CTL_MOD);
                }
            }
        }
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
        exit(EXIT_FAILURE);
    }

    agregar_socket_epoll(public_listen_sock, EPOLLIN, NULL, EPOLL_CTL_ADD);
    agregar_socket_epoll(local_listen_sock, EPOLLIN, NULL, EPOLL_CTL_ADD);
    agregar_socket_epoll(udp_sock, EPOLLIN | EPOLLONESHOT, NULL, EPOLL_CTL_ADD);
    agregar_socket_epoll(timer_sock, EPOLLIN | EPOLLONESHOT, NULL,
                         EPOLL_CTL_ADD);

    int nproc = cant_nucleos();

    pthread_t id[nproc];

    for (int i = 0; i < nproc; i++) {
        pthread_create(&id[i], NULL, gestionar_epoll, NULL);
    }

    pthread_join(id[0], NULL);

    return 0;
}
