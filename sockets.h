#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <unistd.h>

int crear_nonblocking_socket(const char *ip, int port);
int crear_socket_udp_broadcast(int puerto);
int crear_timer_anuncio(int intervalo_segundos);
void anuncio_broadcast(int udp_sock, int puerto_udp);
