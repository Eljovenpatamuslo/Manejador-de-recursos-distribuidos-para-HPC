#define _GNU_SOURCE
#include "estructuras/recursos.h"
#include "estructuras/tablajobs.h"
#include "estructuras/tablanodos.h"
#include "manejadores.h"
#include "sockets.h"
#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/epoll.h>

#define PUBLIC_DIR "127.0.0.1"
#define LOCAL_DIR "127.0.0.2"
#define PUERTO_TCP 12000
#define PUERTO_UDP 12529
#define MAX_EV_EPOLL 64

#define debug(i) fprintf(stderr, "HOLA: %d\n", i);

TablaNodos tablaNodos;
TablaJobs tablaJobs;
RecursosNodo recursos;

int publicListenSocket;
int localListenSocket;
int udpSocket;
int epollFd;
int timerSocket;

static int agregar_socket_epoll(int socket, int evFlags, void *evDataPtr,
                                int epollCtlFlags);
static void manejar_conn_sock_epoll(ClienteConexion *cliente);
static int aceptar_listen_sock_epoll(int eventFd);
static void rearmar_fd_epoll(int eventFd);

static int aceptar_listen_sock_epoll(int eventFd) {
    int connSocket = accept4(eventFd, NULL, NULL, SOCK_NONBLOCK);

    if (connSocket == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 1;
        }
        perror("accept4");
    }

    int tipo =
        eventFd == publicListenSocket ? CLIENTE_AGENTE_C : CLIENTE_ERLANG;

    ClienteConexion *nuevo_cliente = crear_cliente(connSocket, tipo);

    agregar_socket_epoll(connSocket, EPOLLIN | EPOLLONESHOT, nuevo_cliente,
                         EPOLL_CTL_ADD);

    printf("[%s %d] Cliente agregado\n",
           tipo == CLIENTE_AGENTE_C ? "AGENTE C" : "ERLANG", connSocket);

    return 0;
}

int agregar_socket_epoll(int socket, int evFlags, void *evDataPtr,
                         int epollCtlFlags) {
    struct epoll_event ev;
    ev.events = evFlags;

    if (evDataPtr != NULL) {
        ev.data.ptr = evDataPtr;
    } else {
        ev.data.fd = socket;
    }

    if (epoll_ctl(epollFd, epollCtlFlags, socket, &ev) == -1) {
        perror("agregar_socket_epoll: epoll_ctl");
        return -1;
    }
    return 0;
}

void *gestionar_epoll(void *arg) {
    struct epoll_event events[MAX_EV_EPOLL];
    struct epoll_event ev;

    for (int numFds, n;;) {
        numFds = epoll_wait(epollFd, events, MAX_EV_EPOLL, -1);

        if (numFds == -1)
            perror("epoll_wait");

        for (n = 0; n < numFds; ++n) {
            int eventFd = events[n].data.fd;

            if (eventFd == timerSocket) {
                // Debo anunciarme
                manejar_timer(timerSocket, udpSocket, PUERTO_UDP);
                agregar_socket_epoll(eventFd, EPOLLIN | EPOLLONESHOT, NULL,
                                     EPOLL_CTL_MOD);
            }

            else if (eventFd == udpSocket) {
                // Anuncio de otro nodo
                registrar_nodo(udpSocket, tablaNodos);
                agregar_socket_epoll(eventFd, EPOLLIN | EPOLLONESHOT, NULL,
                                     EPOLL_CTL_MOD);
            }

            else if (eventFd == localListenSocket ||
                     eventFd == publicListenSocket) {
                if (aceptar_listen_sock_epoll(eventFd) == 1)
                    continue;
            }

            else {
                ClienteConexion *cliente =
                    (ClienteConexion *)events[n].data.ptr;

                int sockClosed = leer_y_procesar_cliente(cliente);

                if (sockClosed) {
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
    tablaNodos = tablanodos_crear(MAX_NODOS);
    tablaJobs = tablajobs_crear();
    recursos = inicializar_recursos_locales();

    publicListenSocket = crear_nonblocking_socket(PUBLIC_DIR, PUERTO_TCP);
    localListenSocket = crear_nonblocking_socket(LOCAL_DIR, PUERTO_TCP);
    udpSocket = crear_socket_udp_broadcast(PUERTO_UDP);
    timerSocket = crear_timer_anuncio(5);

    epollFd = epoll_create1(0);
    if (epollFd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    agregar_socket_epoll(timerSocket, EPOLLIN | EPOLLONESHOT, NULL,
                         EPOLL_CTL_ADD);

    int nproc = cant_nucleos();

    pthread_t id[nproc];

    printf("Inicio del servidor. Espero dos segundos para escuchar anuncios de "
           "otros nodos\n");
    for (int i = 0; i < nproc; i++) {
        pthread_create(&id[i], NULL, gestionar_epoll, NULL);
    }

    anuncio_broadcast(udpSocket, PUERTO_UDP);
    agregar_socket_epoll(udpSocket, EPOLLIN | EPOLLONESHOT, NULL,
                         EPOLL_CTL_ADD);
    sleep(2);
    printf("Empiezo a escuchar peticiones en los listen sockets\n");
    agregar_socket_epoll(publicListenSocket, EPOLLIN, NULL, EPOLL_CTL_ADD);
    agregar_socket_epoll(localListenSocket, EPOLLIN, NULL, EPOLL_CTL_ADD);

    pthread_join(id[0], NULL);

    return 0;
}
