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
RecursosNodo recNodo;

int socketEscuchaPublica;
int socketEscuchaLocal;
int udpSocket;
int epollFd;
int timerSocket;
int erlangFd;

static void manejar_conn_sock_epoll(ClienteConexion *cliente);
static int aceptar_listen_sock_epoll(int eventFd);
static void rearmar_fd_epoll(int eventFd);

static int aceptar_listen_sock_epoll(int eventFd) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int connSocket = accept4(eventFd, (struct sockaddr *)&client_addr,
                             &client_len, SOCK_NONBLOCK);

    if (connSocket == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 1;
        }
        perror("accept4");
        return -1;
    }

    char ip[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &client_addr.sin_addr, ip, INET_ADDRSTRLEN) ==
        NULL) {
        perror("inet_ntop falló al extraer IP del cliente");
        strcpy(ip, "Desconocida");
    }

    int tipo =
        eventFd == socketEscuchaPublica ? CLIENTE_AGENTE_C : CLIENTE_ERLANG;

    ClienteConexion *nuevo_cliente = crear_cliente(connSocket, tipo, ip, NULL);

    agregar_socket_epoll(epollFd, connSocket, EPOLLIN | EPOLLONESHOT,
                         nuevo_cliente, EPOLL_CTL_ADD);

    printf("[%s %d] Cliente agregado desde IP: %s\n",
           tipo == CLIENTE_AGENTE_C ? "AGENTE C" : "ERLANG", connSocket, ip);

    if (tipo == CLIENTE_ERLANG)
        erlangFd = connSocket;

    return 0;
}

// void *gestionar_epoll(void *arg) {
//     struct epoll_event events[MAX_EV_EPOLL];
//     struct epoll_event ev;
//
//     for (int numFds, n;;) {
//         numFds = epoll_wait(epollFd, events, MAX_EV_EPOLL, -1);
//
//         if (numFds == -1)
//             perror("epoll_wait");
//
//         for (n = 0; n < numFds; ++n) {
//             int eventFd = events[n].data.fd;
//
//             if (eventFd == timerSocket) {
//                 // Debo anunciarme
//                 manejar_timer(timerSocket, udpSocket, PUERTO_UDP);
//                 agregar_socket_epoll(epollFd, eventFd, EPOLLIN |
//                 EPOLLONESHOT,
//                                      NULL, EPOLL_CTL_MOD);
//             }
//
//             else if (eventFd == udpSocket) {
//                 // Anuncio de otro nodo
//                 registrar_nodo(udpSocket, tablaNodos);
//                 agregar_socket_epoll(epollFd, eventFd, EPOLLIN |
//                 EPOLLONESHOT,
//                                      NULL, EPOLL_CTL_MOD);
//             }
//
//             else if (eventFd == socketEscuchaLocal ||
//                      eventFd == socketEscuchaPublica) {
//                 if (aceptar_listen_sock_epoll(eventFd) == 1)
//                     continue;
//             }
//
//             else {
//                 ClienteConexion *cliente =
//                     (ClienteConexion *)events[n].data.ptr;
//
//                 int sockClosed = leer_y_procesar_cliente(
//                     cliente, recNodo, tablaJobs, tablaNodos, erlangFd,
//                     epollFd);
//
//                 if (sockClosed) {
//                     close(cliente->fd);
//                     free(cliente);
//                 } else {
//                     agregar_socket_epoll(epollFd, cliente->fd,
//                                          EPOLLIN | EPOLLONESHOT, cliente,
//                                          EPOLL_CTL_MOD);
//                 }
//             }
//         }
//     }
// }

void *gestionar_epoll(void *arg) {
    struct epoll_event events[MAX_EV_EPOLL];

    for (;;) {
        int numFds = epoll_wait(epollFd, events, MAX_EV_EPOLL, -1);
        if (numFds == -1) {
            if (errno == EINTR)
                continue;
            perror("epoll_wait");
        }

        for (int n = 0; n < numFds; ++n) {
            // Extaer el puntero directamente de forma segura
            ClienteConexion *contexto = (ClienteConexion *)events[n].data.ptr;
            if (contexto == NULL)
                continue;

            int eventFd = contexto->fd;

            if (eventFd == timerSocket) {
                manejar_timer(timerSocket, udpSocket, PUERTO_UDP, recNodo);
                agregar_socket_epoll(epollFd, eventFd, EPOLLIN | EPOLLONESHOT,
                                     contexto, EPOLL_CTL_MOD);
            } else if (eventFd == udpSocket) {
                registrar_nodo(udpSocket, tablaNodos);
                agregar_socket_epoll(epollFd, eventFd, EPOLLIN | EPOLLONESHOT,
                                     contexto, EPOLL_CTL_MOD);
            } else if (eventFd == socketEscuchaLocal ||
                       eventFd == socketEscuchaPublica) {
                aceptar_listen_sock_epoll(eventFd);
                agregar_socket_epoll(epollFd, eventFd, EPOLLIN, contexto,
                                     EPOLL_CTL_MOD);
            } else {
                int sockClosed =
                    leer_y_procesar_cliente(contexto, recNodo, tablaJobs,
                                            tablaNodos, erlangFd, epollFd);

                if (sockClosed) {
                    close(contexto->fd);
                    free(contexto);
                } else {
                    agregar_socket_epoll(epollFd, contexto->fd,
                                         EPOLLIN | EPOLLONESHOT, contexto,
                                         EPOLL_CTL_MOD);
                }
            }
        }
    }
}

int main() {
    tablaNodos = tablanodos_crear(MAX_NODOS);
    tablaJobs = tablajobs_crear();
    recNodo = inicializar_recursos_locales();

    socketEscuchaPublica =
        crear_nonblocking_listen_socket(PUBLIC_DIR, PUERTO_TCP);
    socketEscuchaLocal = crear_nonblocking_listen_socket(LOCAL_DIR, PUERTO_TCP);
    udpSocket = crear_socket_udp_broadcast(PUERTO_UDP);
    timerSocket = crear_timer_anuncio(5);

    ClienteConexion *ctxTimer =
        crear_cliente(timerSocket, CLIENTE_AGENTE_C, "127.0.0.1", NULL);
    ClienteConexion *ctxUdp =
        crear_cliente(udpSocket, CLIENTE_AGENTE_C, "0.0.0.0", NULL);
    ClienteConexion *ctxPublico =
        crear_cliente(socketEscuchaPublica, CLIENTE_AGENTE_C, PUBLIC_DIR, NULL);
    ClienteConexion *ctxLocal =
        crear_cliente(socketEscuchaLocal, CLIENTE_ERLANG, LOCAL_DIR, NULL);

    epollFd = epoll_create1(0);
    if (epollFd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    agregar_socket_epoll(epollFd, timerSocket, EPOLLIN | EPOLLONESHOT, ctxTimer,
                         EPOLL_CTL_ADD);

    int nproc = cant_nucleos();

    pthread_t id[nproc];

    printf("Inicio del servidor. Espero dos segundos para escuchar anuncios de "
           "otros nodos\n");

    for (int i = 0; i < nproc; i++) {
        pthread_create(&id[i], NULL, gestionar_epoll, NULL);
    }

    anuncio_broadcast(udpSocket, PUERTO_UDP, recNodo);
    agregar_socket_epoll(epollFd, udpSocket, EPOLLIN | EPOLLONESHOT, ctxUdp,
                         EPOLL_CTL_ADD);
    sleep(2);
    printf("Empiezo a escuchar peticiones en los listen sockets\n");
    agregar_socket_epoll(epollFd, socketEscuchaPublica, EPOLLIN, ctxPublico,
                         EPOLL_CTL_ADD);
    agregar_socket_epoll(epollFd, socketEscuchaLocal, EPOLLIN, ctxLocal,
                         EPOLL_CTL_ADD);

    pthread_join(id[0], NULL);

    return 0;
}
