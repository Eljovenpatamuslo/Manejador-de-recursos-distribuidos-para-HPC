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
char *ipBroadcast;
char *miIp;
char *dirLocal;
int puertoTcp;

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
                manejar_timer(timerSocket, PUERTO_UDP, puertoTcp, miIp,
                              ipBroadcast);
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
                int sockClosed = leer_y_procesar_cliente(
                contexto, recNodo, tablaJobs, tablaNodos, erlangFd, epollFd,miIp);

                if (sockClosed) {
                    // Si es un agente C, limpiar sus jobs y solicitudes pendientes
                    if (contexto->tipo == CLIENTE_AGENTE_C) {
                        DatosNodo *datos = tablanodos_buscar(tablaNodos, contexto->ip);
                        if (datos != NULL) {
                            tablajobs_borrar_por_nodo(tablaJobs, contexto->ip, 
                                                     datos->puerto, recNodo);
                        }
                    }
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

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr,
                "Uso: %s <MI_IP> <MI_PUERTO_TCP> <IP_BROADCAST> <DIR_LOCAL>\n",
                argv[0]);
        fprintf(stderr,
                "Ejemplo SIMULACION: %s 127.0.0.2 12000 127.255.255.255 "
                "127.0.0.1\n",
                argv[0]);
        fprintf(stderr,
                "Ejemplo RED REAL:   %s 192.168.0.14 12000 255.255.255.255 "
                "127.0.0.1\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }

    miIp = argv[1];
    puertoTcp = atoi(argv[2]);
    ipBroadcast = argv[3];
    dirLocal = argv[4];

    tablaNodos = tablanodos_crear(MAX_NODOS);
    tablaJobs = tablajobs_crear();
    recNodo = inicializar_recursos_locales();

    socketEscuchaPublica = crear_nonblocking_listen_socket(miIp, puertoTcp);

    socketEscuchaLocal = crear_nonblocking_listen_socket(dirLocal, puertoTcp);
    udpSocket = crear_socket_udp_broadcast(PUERTO_UDP);
    timerSocket = crear_timer_anuncio(5);

    ClienteConexion *ctxTimer =
        crear_cliente(timerSocket, CLIENTE_AGENTE_C, "127.0.0.1", NULL);
    ClienteConexion *ctxUdp =
        crear_cliente(udpSocket, CLIENTE_AGENTE_C, "0.0.0.0", NULL);
    ClienteConexion *ctxPublico =
        crear_cliente(socketEscuchaPublica, CLIENTE_AGENTE_C, miIp, NULL);

    ClienteConexion *ctxLocal =
        crear_cliente(socketEscuchaLocal, CLIENTE_ERLANG, dirLocal, NULL);

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

    anuncio_broadcast(PUERTO_UDP, puertoTcp, miIp, ipBroadcast);
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
