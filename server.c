#define _GNU_SOURCE
#include "sockets.h"
#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#define PUBLIC_DIR "127.0.0.2"
#define PRIVATE_DIR "127.0.0.2"
#define PUERTO_C 8100
#define MAX_EV_EPOLL 64

int public_listen_sock;
int local_listen_sock;
int epollfd;

typedef enum { CLIENTE_ERLANG, CLIENTE_AGENTE_C } TipoCliente;

typedef struct {
    int fd;
    TipoCliente tipo;
} ClienteConexion;

static void manejar_conn_sock_epoll(ClienteConexion *cliente);
static int aceptar_listen_sock_epoll(int eventfd);

void *gestionar_epoll(void *arg) {
    struct epoll_event events[MAX_EV_EPOLL];
    struct epoll_event ev;

    for (int num_fds, n;;) {
        num_fds = epoll_wait(epollfd, events, MAX_EV_EPOLL, -1);
        if (num_fds == -1)
            perror("epoll_wait");

        for (n = 0; n < num_fds; ++n) {
            int eventfd = events[n].data.fd;
            if (eventfd == local_listen_sock || eventfd == public_listen_sock) {
                if (aceptar_listen_sock_epoll(eventfd) == 1)
                    continue;
            }

            else {
                ClienteConexion *cliente = events[n].data.ptr;
                manejar_conn_sock_epoll(cliente);
                // int sock_closed = handle_conn(conn_sock);
                // if (!sock_closed) {
                //     ev.events = EPOLLIN | EPOLLONESHOT;
                //     ev.data.fd = conn_sock;
                //     if (epoll_ctl(epollfd, EPOLL_CTL_MOD, conn_sock, &ev) ==
                //         -1) {
                //         perror("epoll_ctl: conn_sock");
                //         exit(EXIT_FAILURE);
                //     }
                // }
            }
        }
    }
}

static void manejar_conn_sock_epoll(ClienteConexion *cliente) {
    if (cliente->tipo == CLIENTE_AGENTE_C) {
        manejar_agente_c(cliente);
    } else {
        manejar_erlang(cliente);
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

void agregar_listen_sock_epoll(int listen_sock) {
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLEXCLUSIVE;
    ev.data.fd = listen_sock;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }
}

int main() {
    public_listen_sock = crear_nonblocking_socket(PUBLIC_DIR, PUERTO_C);
    local_listen_sock = crear_nonblocking_socket(PUBLIC_DIR, PUERTO_C);

    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("epoll_create1");
    }

    agregar_listen_sock_epoll(public_listen_sock);
    agregar_listen_sock_epoll(local_listen_sock);

    int nproc = cant_nucleos();

    pthread_t id[nproc];

    for (int i = 0; i < nproc; i++) {
        pthread_create(&id[i], NULL, gestionar_epoll, NULL);
    }

    pthread_join(id[0], NULL);

    return 0;
}
