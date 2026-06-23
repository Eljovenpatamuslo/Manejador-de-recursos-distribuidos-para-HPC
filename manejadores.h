#include "estructuras/recursos.h"
#include "estructuras/tablajobs.h"
#include "estructuras/tablanodos.h"
#include "sockets.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_MSG_LEN 256

typedef enum {
    CLIENTE_ERLANG,
    CLIENTE_AGENTE_C,
    CONEXION_SALIENTE
} TipoCliente;

typedef struct {
    int fd;
    TipoCliente tipo;
    char buffer[4096];
    int bytes_in_buffer;
    char ip[16];
    char mensaje[256];
} ClienteConexion;

ClienteConexion *crear_cliente(int clienteFD, int tipo, char ip[],
                               char *mensaje);

int leer_y_procesar_cliente(ClienteConexion *cliente, RecursosNodo recNodo,
                            TablaJobs tablaJobs, TablaNodos tablaNodos,
                            int erlangSchedulerSocket, int epollFd);

void manejar_agente_c(ClienteConexion *cliente, const char *mensaje,
                      RecursosNodo recNodo, TablaJobs tablaJobs,
                      TablaNodos tablaNodos, int erlangSchedulerSocket);

void manejar_cliente_erlang(ClienteConexion *cliente, const char *mensaje,
                            TablaJobs tablaJobs, TablaNodos tablaNodos,
                            int epollFd, RecursosNodo recNodo);

void manejar_timer(int timerSocket, int udp_sock, int puerto_udp,
                   RecursosNodo recNodo, int puertoTcpEscucha,
                   const char *miIp);

void registrar_nodo(int udp_sock, TablaNodos tablaNodos);

void anuncio_broadcast(int udp_sock, int puerto_udp, RecursosNodo recNodo,
                       int puertoTcpEscucha, const char *miIp);
