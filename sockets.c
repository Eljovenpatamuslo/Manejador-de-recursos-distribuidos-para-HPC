#include "sockets.h"

int crear_nonblocking_socket(const char *ip, int port) {
    int sfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sfd == -1) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(sfd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sfd);
        return -1;
    }

    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(sfd);
        return -1;
    }

    // SOMAXCONN tamaño máximo de cola
    if (listen(sfd, SOMAXCONN) == -1) {
        perror("listen");
        close(sfd);
        return -1;
    }

    return sfd;
}

int crear_socket_udp_broadcast(int puerto) {
    int udp_sock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (udp_sock == -1) {
        perror("socket udp");
        return -1;
    }

    int broadcast_enable = 1;
    if (setsockopt(udp_sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable,
                   sizeof(broadcast_enable)) == -1) {
        perror("setsockopt SO_BROADCAST");
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
        perror("bind udp");
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

void anuncio_broadcast(int udp_sock, int puerto_udp) {
    char mensaje_anuncio[256];
    snprintf(mensaje_anuncio, sizeof(mensaje_anuncio),
             "ANNOUNCE puerto recursos\n");

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(puerto_udp);
    dest_addr.sin_addr.s_addr =
        inet_addr("255.255.255.255"); // IP de Broadcast universal

    sendto(udp_sock, mensaje_anuncio, strlen(mensaje_anuncio), 0,
           (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    printf("Anuncio broadcast enviado.\n");
}
