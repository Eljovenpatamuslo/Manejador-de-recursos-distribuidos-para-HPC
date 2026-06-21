#include "estructuras/tablajobs.h"
#include "estructuras/tablanodos.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum { CLIENTE_ERLANG, CLIENTE_AGENTE_C } TipoCliente;

typedef struct {
    int fd;
    TipoCliente tipo;
    char buffer[4096];
    int bytes_in_buffer;
    char ip[16];
} ClienteConexion;

ClienteConexion *crear_cliente(int eventfd, int tipo, char ip[]);
int leer_y_procesar_cliente(ClienteConexion *cliente, RecursosNodo recNodo,
                            TablaJobs tablaJobs, TablaNodos tablaNodos);
void manejar_agente_c(ClienteConexion *cliente, const char *mensaje,
                      RecursosNodo recNodo, TablaJobs tablaJobs, TablaNodos);
void manejar_cliente_erlang(ClienteConexion *cliente, const char *mensaje);
void manejar_timer(int timerSocket, int udp_sock, int puerto_udp);
void registrar_nodo(int udp_sock, TablaNodos tablaNodos);
void anuncio_broadcast(int udp_sock, int puerto_udp);
