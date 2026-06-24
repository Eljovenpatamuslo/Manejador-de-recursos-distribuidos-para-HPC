#include "estructuras/recursos.h"
#include "estructuras/tablajobs.h"
#include "estructuras/tablanodos.h"
#include "estructuras/utils.h"
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

/*
 * Reserva memoria en el heap para un contexto ClienteConexion.
 * Inicializa el FD, tipo de conexión, y almacena datos vitales (IP, puerto,
 * buffer de mensajes) para rastrear el estado del socket dentro del ciclo
 * epoll.
 */
ClienteConexion *crear_cliente(int clienteFD, int tipo, char ip[],
                               char *mensaje);

/*
 * Consume el socket no bloqueante hasta vaciarlo (EAGAIN), acumulando bytes en
 * el buffer del contexto. Tokeniza el flujo por saltos de línea ('\n') y rutea
 * los comandos completos al manejador específico según su origen.
 */
int leer_y_procesar_cliente(ClienteConexion *cliente, RecursosNodo recNodo,
                            TablaJobs tablaJobs, TablaNodos tablaNodos,
                            int erlangSchedulerSocket, int epollFd);

/*
 * Parsea y ejecuta los protocolos remotos entre nodos (RESERVE, GRANTED,
 * DENIED, RELEASE). Modifica las estructuras concurrentes (TablaJobs,
 * RecursosNodo) y notifica los cambios de estado correspondientes al
 * planificador Erlang local.
 */
void manejar_agente_c(ClienteConexion *cliente, const char *mensaje,
                      RecursosNodo recNodo, TablaJobs tablaJobs,
                      TablaNodos tablaNodos, int erlangSchedulerSocket);

/*
 * Intérprete de las órdenes locales del planificador. Dinámicamente instancia
 * sockets efímeros asíncronos para dependencias remotas (JOB_REQUEST), expone
 * la topología de red actual (GET_NODES) y gestiona la liberación de hardware
 * (JOB_RELEASE).
 */
void manejar_cliente_erlang(ClienteConexion *cliente, const char *mensaje,
                            TablaJobs tablaJobs, TablaNodos tablaNodos,
                            int epollFd, RecursosNodo recNodo);

/*
 * Consumidor del evento timerfd. Drena los bytes de expiración del socket
 * para evitar re-disparos continuos en epoll y dispara la función de broadcast
 * para emitir el estado actualizado del hardware local.
 */
void manejar_timer(int timerSocket, int udp_sock, int puerto_udp,
                   RecursosNodo recNodo, int puertoTcpEscucha, const char *miIp,
                   const char *ip_broadcast);

/*
 * Procesa datagramas UDP entrantes. Extrae la IP de la cabecera de red y parsea
 * la carga útil (ANNOUNCE) para inyectar o actualizar dinámicamente la
 * información de capacidad y ruteo en la TablaNodos global.
 */
void registrar_nodo(int udp_sock, TablaNodos tablaNodos);

/*
 * Empaqueta el estado del hardware local en un mensaje de texto. Crea un socket
 * UDP efímero de salida, lo ata a la IP de origen local y dispara el datagrama
 * hacia la IP de broadcast especificada para notificar a la subred.
 */
void anuncio_broadcast(int puerto_udp, RecursosNodo recNodo,
                       int puertoTcpEscucha, const char *mi_ip,
                       const char *ip_broadcast);
