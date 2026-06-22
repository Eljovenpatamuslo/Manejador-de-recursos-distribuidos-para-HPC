#include "sockets.h"

int crear_nonblocking_listen_socket(const char *ip, int port) {
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

int crear_socket_saliente_nobloqueante(const char *ip, unsigned short puerto) {
    // 1. Creamos el socket TCP y le inyectamos la flag SOCK_NONBLOCK
    // directamente
    int sock_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sock_fd == -1) {
        perror("Error al crear socket de salida no bloqueante");
        return -1;
    }

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(puerto);

    if (inet_pton(AF_INET, ip, &dest_addr.sin_addr) <= 0) {
        perror("Error en inet_pton: IP inválida");
        close(sock_fd);
        return -1;
    }

    // 2. Intentamos conectar. Al ser no bloqueante, no va a esperar el
    // handshake.
    if (connect(sock_fd, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) ==
        -1) {
        // Si el error es EINPROGRESS, todo está perfecto. Se está conectando de
        // fondo.
        if (errno != EINPROGRESS) {
            perror("Fallo crítico en connect hacia el otro nodo");
            close(sock_fd);
            return -1;
        }
    }

    // Devolvemos el descriptor. Aún no está listo para usarse, epoll nos
    // avisará.
    return sock_fd;
}

int agregar_socket_epoll(int epollFd, int socket, int evFlags, void *evDataPtr,
                         int epollCtlFlags) {
    struct epoll_event ev;
    ev.events = evFlags;

    if (evDataPtr != NULL) {
        ev.data.ptr = evDataPtr;
    } else {
        ev.data.fd = socket;
    }

    if (epoll_ctl(epollFd, epollCtlFlags, socket, &ev) == -1) {
        perror("agregar_socket_epoll: epoll_ctl");
        return -1;
    }
    return 0;
}
