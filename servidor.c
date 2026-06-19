#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080
#define MAX_EVENTS 10

// Función auxiliar para hacer un socket no bloqueante
int make_socket_non_blocking(int sfd) {
    int flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return -1;
    }
    flags |= O_NONBLOCK;
    if (fcntl(sfd, F_SETFL, flags) == -1) {
        perror("fcntl F_SETFL");
        return -1;
    }
    return 0;
}

// Función para crear, configurar y poner a la escucha un socket
int create_and_bind_socket(const char *ip, int port) {
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) {
        perror("socket");
        return -1;
    }

    // Permitir reuso del puerto
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

    if (make_socket_non_blocking(sfd) == -1) {
        close(sfd);
        return -1;
    }

    if (listen(sfd, SOMAXCONN) == -1) {
        perror("listen");
        close(sfd);
        return -1;
    }

    return sfd;
}

int main() {
    // 1. Crear los dos sockets de escucha
    // NOTA: Reemplaza "192.168.1.50" con la IP pública/LAN real de tu máquina
    int public_sfd = create_and_bind_socket("192.168.1.50", PORT);
    if (public_sfd == -1)
        exit(EXIT_FAILURE);
    printf("Escuchando agentes externos en IP pública, puerto %d\n", PORT);

    int local_sfd = create_and_bind_socket("127.0.0.1", PORT);
    if (local_sfd == -1)
        exit(EXIT_FAILURE);
    printf("Escuchando a Erlang en localhost, puerto %d\n", PORT);

    // 2. Inicializar epoll
    int efd = epoll_create1(0);
    if (efd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    struct epoll_event event;
    struct epoll_event events[MAX_EVENTS];

    // Registrar socket público para lectura
    event.data.fd = public_sfd;
    event.events = EPOLLIN | EPOLLET; // Edge-Triggered
    if (epoll_ctl(efd, EPOLL_CTL_ADD, public_sfd, &event) == -1) {
        perror("epoll_ctl public");
        exit(EXIT_FAILURE);
    }

    // Registrar socket local para lectura
    event.data.fd = local_sfd;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, local_sfd, &event) == -1) {
        perror("epoll_ctl local");
        exit(EXIT_FAILURE);
    }

    // 3. El bucle de eventos
    printf("Iniciando bucle epoll...\n");
    while (1) {
        int n = epoll_wait(efd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) ||
                (events[i].events & EPOLLHUP) ||
                (!(events[i].events & EPOLLIN))) {
                fprintf(stderr, "Error en epoll en el fd %d\n",
                        events[i].data.fd);
                close(events[i].data.fd);
                continue;
            }

            if (public_sfd == events[i].data.fd) {
                // Hay una conexión entrante de un agente externo
                printf("Nueva conexión en el socket público.\n");
                // Aquí iría el bucle de accept()
            } else if (local_sfd == events[i].data.fd) {
                // Hay una conexión entrante de Erlang
                printf("Nueva conexión en el socket local (Erlang).\n");
                // Aquí iría el bucle de accept()
            } else {
                // Datos listos para leer en un socket ya conectado
                printf("Datos disponibles en el descriptor %d\n",
                       events[i].data.fd);
                // Aquí iría la lógica de read()
            }
        }
    }

    close(public_sfd);
    close(local_sfd);
    return 0;
}
