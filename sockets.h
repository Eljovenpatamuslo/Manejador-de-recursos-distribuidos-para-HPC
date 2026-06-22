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

int crear_socket_saliente_nobloqueante(const char *ip, unsigned short puerto);
int crear_nonblocking_listen_socket(const char *ip, int port);
int crear_socket_udp_broadcast(int puerto);
int crear_timer_anuncio(int intervalo_segundos);

int agregar_socket_epoll(int epollFd, int socket, int evFlags, void *evDataPtr,
                         int epollCtlFlags);
