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
