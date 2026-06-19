#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_ELEMENTOS 10000
#define MAX_EVENTOS 1000
#define MAX_HILOS 4
/*
 * Para probar, usar netcat. Ej:
 *
 * $ nc localhost 3942
 */

typedef struct {
        char *k;
        char *v;
} kv;

typedef struct {
        kv *arr;
        int tam;
        int ult;
} _vec;

typedef _vec *vect;
vect map;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int listen_sock;
int epollfd;

vect inicializar(int s) {
    vect vec = malloc(sizeof(_vec));
    vec->arr = malloc(s * sizeof(kv));
    vec->tam = s;
    vec->ult = -1;
    return vec;
}

void insertar(vect vec, char *key, char *val) {
    for (int i = 0; i <= vec->ult; i++) {
        if (strcmp(key, vec->arr[i].k) == 0) {
            free(vec->arr[i].v);
            vec->arr[i].v = strdup(val);
            return;
        }
    }

    if (vec->ult + 1 == vec->tam) {
        vec->tam *= 2;
        vec->arr = realloc(vec->arr, sizeof(kv) * vec->tam);
    }

    vec->ult++;
    vec->arr[vec->ult].k = strdup(key);
    vec->arr[vec->ult].v = strdup(val);
}

char *buscar(vect vec, char *key) {
    for (int i = 0; i <= vec->ult; i++) {
        if (strcmp(key, vec->arr[i].k) == 0)
            return vec->arr[i].v;
    }
    return NULL;
}

void eliminar(vect vec, char *key) {
    for (int i = 0; i <= vec->ult; i++) {
        if (!strcmp(key, vec->arr[i].k)) {
            free(vec->arr[i].v);
            vec->arr[i].v = NULL;
        }
    }
}

void quit(char *s) {
    perror(s);
    abort();
}

int fd_readline(int fd, char *buf) {
    int rc;
    int i = 0;

    /*
     * Leemos de a un caracter (no muy eficiente...) hasta
     * completar una línea.
     */
    while ((rc = read(fd, buf + i, 1)) > 0) {
        if (buf[i] == '\n')
            break;
        i++;
    }

    if (rc < 0)
        return rc;

    buf[i] = 0;
    return i;
}

int handle_conn(int csock) {
    char buf[200];
    char reply[200];
    int rc;

    char *ptr = NULL;

    /* Atendemos pedidos, uno por linea */
    rc = fd_readline(csock, buf);

    if (rc > 0) {
        char *comando = strtok_r(buf, " \n\0", &ptr);

        if (!strcmp(comando, "PUT")) {
            char *key = strtok_r(NULL, " \n\0", &ptr);
            char *value = strtok_r(NULL, " \n\0", &ptr);
            pthread_mutex_lock(&mutex);
            insertar(map, key, value);
            pthread_mutex_unlock(&mutex);

            write(csock, "OK\n", 3);
        }

        else if (!strcmp(comando, "GET")) {
            char *val;
            char *key = strtok_r(NULL, " \n\0", &ptr);
            pthread_mutex_lock(&mutex);
            val = buscar(map, key);
            pthread_mutex_unlock(&mutex);
            if (val) {
                sprintf(reply, "OK %s\n", val);
                write(csock, reply, strlen(reply));
            } else
                write(csock, "NOTFOUND\n", 9);
        }

        else if (!strcmp(comando, "DEL")) {
            char *key = strtok_r(NULL, " \n\0", &ptr);
            pthread_mutex_lock(&mutex);
            eliminar(map, key);
            pthread_mutex_unlock(&mutex);
            write(csock, "OK\n", 3);
        }

        else {
            write(csock, "EINVAL\n", 7);
        }
    }

    if (rc < 0)
        quit("read... raro");

    if (rc == 0) {
        /* linea vacia, se cerró la conexión */
        //        struct epoll_event ev;
        //        if (epoll_ctl(epollfd, EPOLL_CTL_DEL, csock, &ev) == -1) {
        //            perror("epoll_ctl: csock");
        //            exit(EXIT_FAILURE);
        //        }
        close(csock);
        return 1;
    }

    return 0;
}

/* Crea un socket de escucha en puerto 3942 TCP */
int mk_lsock() {
    struct sockaddr_in sa;
    int lsock;
    int rc;
    int yes = 1;

    /* Crear socket */
    lsock = socket(AF_INET, SOCK_STREAM, 0);
    if (lsock < 0)
        quit("socket");

    /* Setear opción reuseaddr... normalmente no es necesario */
    if (setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == 1)
        quit("setsockopt");

    sa.sin_family = AF_INET;
    sa.sin_port = htons(3942);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Bindear al puerto 3942 TCP, en todas las direcciones disponibles */
    rc = bind(lsock, (struct sockaddr *)&sa, sizeof sa);
    if (rc < 0)
        quit("bind");

    /* Setear en modo escucha */
    rc = listen(lsock, 10);
    if (rc < 0)
        quit("listen");

    return lsock;
}

void *gestionar_epoll(void *arg) {
    struct epoll_event events[MAX_EVENTOS];
    struct epoll_event ev;

    for (int nfds, n, conn_sock;;) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTOS, -1);
        if (nfds == -1)
            quit("epoll_wait");

        for (n = 0; n < nfds; ++n) {
            if (events[n].data.fd == listen_sock) {
                conn_sock = accept(listen_sock, NULL, NULL);
                if (conn_sock == -1)
                    quit("accept");

                ev.events = EPOLLIN | EPOLLONESHOT;
                ev.data.fd = conn_sock;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev) == -1) {
                    perror("epoll_ctl: conn_sock");
                    exit(EXIT_FAILURE);
                }

            }

            else {
                conn_sock = events[n].data.fd;
                int sock_closed = handle_conn(conn_sock);
                if (!sock_closed) {
                    ev.events = EPOLLIN | EPOLLONESHOT;
                    ev.data.fd = conn_sock;
                    if (epoll_ctl(epollfd, EPOLL_CTL_MOD, conn_sock, &ev) == -1) {
                        perror("epoll_ctl: conn_sock");
                        exit(EXIT_FAILURE);
                    }
                }
            }
        }
    }
}

int main() {
    map = inicializar(MAX_ELEMENTOS);
    listen_sock = mk_lsock();

    struct epoll_event ev;
    epollfd = epoll_create1(0);

    if (epollfd == -1) {
        quit("epoll_create1");
    }

    ev.events = EPOLLIN | EPOLLEXCLUSIVE;
    ev.data.fd = listen_sock;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    pthread_t id[MAX_HILOS];

    for (int i = 0; i < MAX_HILOS; i++) {
        pthread_create(&id[i], NULL, gestionar_epoll, NULL);
    }

    pthread_join(id[0], NULL);

    return 0;
}
