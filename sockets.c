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
