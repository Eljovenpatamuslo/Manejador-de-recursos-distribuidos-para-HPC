#include "sockets.h"

int crear_nonblocking_socket(const char *ip, int port) {
    int sfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sfd == -1) {
        perror("crear_nonblocking_socket: socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("crear_nonblocking_socket: setsockopt");
        close(sfd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        perror("crear_nonblocking_socket: inet_pton");
        close(sfd);
        return -1;
    }

    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("crear_nonblocking_socket: bind");
        close(sfd);
        return -1;
    }

    // SOMAXCONN tamaño máximo de cola
    if (listen(sfd, SOMAXCONN) == -1) {
        perror("crear_nonblocking_socket: listen");
        close(sfd);
        return -1;
    }

    return sfd;
}

int crear_socket_udp_broadcast(int puerto) {
    int udp_sock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (udp_sock == -1) {
        perror("crear_socket_udp_broadcast: socket udp");
        return -1;
    }

    int broadcast_enable = 1;
    if (setsockopt(udp_sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable,
                   sizeof(broadcast_enable)) == -1) {
        perror("crear_socket_udp_broadcast: setsockopt SO_BROADCAST");
        close(udp_sock);
        return -1;
    }

    int opt = 1;
    setsockopt(udp_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(puerto);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udp_sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("crear_socket_udp_broadcast: bind udp");
        close(udp_sock);
        return -1;
    }

    return udp_sock;
}

int crear_timer_anuncio(int intervalo_segundos) {
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (tfd == -1) {
        perror("timerfd_create");
        return -1;
    }

    struct itimerspec ts;
    // Primera vez que se dispara
    ts.it_value.tv_sec = intervalo_segundos;
    ts.it_value.tv_nsec = 0;
    // Intervalo de repetición
    ts.it_interval.tv_sec = intervalo_segundos;
    ts.it_interval.tv_nsec = 0;

    if (timerfd_settime(tfd, 0, &ts, NULL) == -1) {
        perror("timerfd_settime");
        close(tfd);
        return -1;
    }

    return tfd;
}
