#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <unistd.h>

/*
 * Instancia un socket TCP efímero en modo no bloqueante e inicia una conexión
 * asíncrona (connect) hacia el destino. Retorna el FD inmediatamente,
 * típicamente con estado EINPROGRESS, para que epoll notifique cuando el
 * handshake finalice.
 */
int crear_socket_saliente_nobloqueante(const char *ip_destino,
                                       unsigned short puerto_destino,
                                       const char *miIp);

/*
 * Crea un socket TCP servidor pasivo y no bloqueante. Lo enlaza (bind) a la IP
 * y puerto indicados, configurando SO_REUSEADDR para prevenir bloqueos del
 * sistema, y lo deja en estado de escucha (listen) con la cola máxima del
 * kernel (SOMAXCONN).
 */
int crear_nonblocking_listen_socket(const char *ip, int port);

/*
 * Construye un socket UDP (datagrama) sin conexión. Habilita los permisos del
 * kernel para difusión (SO_BROADCAST) y la concurrencia en el mismo puerto
 * (SO_REUSEPORT/ADDR), atándolo a todas las interfaces locales (INADDR_ANY).
 */
int crear_socket_udp_broadcast(int puerto);

/*
 * Genera un temporizador a nivel de kernel utilizando la API timerfd de Linux
 * (basado en el reloj monotónico). Retorna un descriptor no bloqueante que
 * epoll interpretará como un evento de lectura (EPOLLIN) cada vez que el
 * intervalo expire.
 */
int crear_timer_anuncio(int intervalo_segundos);

/*
 * Función envoltorio (wrapper) para la llamada al sistema epoll_ctl.
 * Permite registrar (ADD), modificar (MOD) o eliminar (DEL) la vigilancia de un
 * FD en la cola de epoll, vinculándole una máscara de eventos y un puntero de
 * contexto.
 */
int agregar_socket_epoll(int epollFd, int socket, int evFlags, void *evDataPtr,
                         int epollCtlFlags);
