// publisher_tcp.c

#include <netdb.h>          // getaddrinfo(), freeaddrinfo()
#include <stdio.h>          // printf(), perror()
#include <stdlib.h>         // exit(), strtol()
#include <string.h>         // memset(), strlen(), snprintf()
#include <sys/socket.h>     // socket(), connect(), send()
#include <sys/types.h>      // tipos de socket
#include <time.h>           // time(), nanosleep(), struct timespec
#include <unistd.h>         // close()

static int connect_tcp(const char *host, const char *port) {
    struct addrinfo hints, *res, *rp;
    int fd = -1;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &res) != 0) {
        perror("getaddrinfo");
        exit(1);
    }
    for (rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) {
        perror("connect");
        exit(1);
    }
    return fd;
}

static void msleep(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

int main(int argc, char **argv) {
    const char *host = (argc > 1) ? argv[1] : "127.0.0.1";
    const char *port = (argc > 2) ? argv[2] : "5555";
    const char *subject = (argc > 3) ? argv[3] : "test";
    long interval_ms = (argc > 4) ? strtol(argv[4], NULL, 10) : 1000;

    int fd = connect_tcp(host, port);
    printf("Publisher connected to %s:%s, subject='%s', every %ld ms.\n", host, port, subject, interval_ms);

    const char *role = "PUB\n";
    (void) send(fd, role, strlen(role), 0);

    unsigned long counter = 0;
    char header[256];
    char payload[1024];
    while (1) {
        time_t now = time(NULL);
        int plen = snprintf(payload, sizeof(payload), "msg %lu at %ld", counter++, (long)now);
        int hlen = snprintf(header, sizeof(header), "PUBLISH %s %d\n", subject, plen);
        if (send(fd, header, (size_t) hlen, 0) < 0) {
            perror("send header");
            break;
        }
        if (send(fd, payload, (size_t) plen, 0) < 0) {
            perror("send payload");
            break;
        }
        printf("Sent message number %lu to subject '%s'\n", counter - 1, subject);
        msleep(interval_ms);
    }
    close(fd);
    return 0;
}

/*
===============================================================================
Explicación detallada de las librerías usadas (y dónde se usan)
-------------------------------------------------------------------------------
<netdb.h>
  - getaddrinfo()/freeaddrinfo() para resolver el host:puerto del broker TCP.

<stdio.h>
  - printf() para mensajes de estado; perror() para reportar fallos de sistema.

<stdlib.h>
  - exit() ante errores fatales; strtol() para convertir el intervalo en ms.

<string.h>
  - memset(), strlen(), snprintf() para preparar buffers de cabecera/payload.

<sys/socket.h>
  - socket(), connect(), send(): API TCP para establecer conexión y enviar.

<sys/types.h>
  - Tipos auxiliares requeridos por la API de sockets.

<time.h>
  - time() para timestamp en el mensaje; nanosleep() y struct timespec para
    implementar msleep().

<unistd.h>
  - close() para cerrar el socket cuando se termina el bucle.
===============================================================================
*/
