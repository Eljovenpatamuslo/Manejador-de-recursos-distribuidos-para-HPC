#ifndef CONEXION_H
#define CONEXION_H

#include <netinet/in.h>   // INET_ADDRSTRLEN

typedef enum {
    CLIENTE_ERLANG,
    CLIENTE_AGENTE_C,
    CONEXION_SALIENTE
} TipoCliente;

typedef struct {
    int fd;
    TipoCliente tipo;
    char ip[16];
    char mensaje[256];
    char buffer[1024];
    int bytes_in_buffer;
    int tipoRec;          // TipoRecurso, usado en GRANTED
} ClienteConexion;

/*
 * Reserva memoria en el heap para un contexto ClienteConexion.
 * Inicializa el FD, tipo de conexión, y almacena datos vitales (IP, puerto,
 * buffer de mensajes) para rastrear el estado del socket dentro del ciclo
 * epoll.
 */
ClienteConexion *crear_cliente(int clienteFD, int tipo, const char ip[], char *mensaje);

#endif