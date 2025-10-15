// publisher_tcp.c

#include <netdb.h>          // getaddrinfo(), freeaddrinfo()
#include <stdio.h>          // printf(), perror()
#include <stdlib.h>         // exit(), strtol()
#include <string.h>         // memset(), strlen(), snprintf()
#include <sys/socket.h>     // socket(), connect(), send()
#include <sys/types.h>      // tipos de socket
#include <time.h>           // time(), nanosleep(), struct timespec
#include <unistd.h>         // close()

// Función para conectar a un servidor TCP.
static int connect_tcp(const char *host, const char *port) {
    struct addrinfo hints, *res, *rp; // punteros para recorrer resultados
    int fd = -1; // descriptor de socket
    memset(&hints, 0, sizeof(hints)); // inicializar a 0
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP
    // Resuelve el nombre de host y el puerto.
    if (getaddrinfo(host, port, &hints, &res) != 0) {
        perror("getaddrinfo");
        exit(1);
    }
    // Itera sobre las direcciones resueltas e intenta conectar.
    for (rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue; // Si falla la creación del socket, prueba con la siguiente dirección.
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break; // Si la conexión es exitosa, sale del bucle.
        close(fd); // Si la conexión falla, cierra el socket.
        fd = -1;
    }
    freeaddrinfo(res); // Libera la memoria de las direcciones resueltas.
    if (fd < 0) {
        // Si no se pudo conectar a ninguna dirección.
        perror("connect");
        exit(1);
    }
    return fd; // Devuelve el descriptor del socket conectado.
}

// Función para pausar la ejecución durante un número de milisegundos.
static void msleep(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

int main(int argc, char **argv) {
    // Obtiene los parámetros de la línea de comandos o usa valores por defecto.
    const char *host = (argc > 1) ? argv[1] : "127.0.0.1";
    const char *port = (argc > 2) ? argv[2] : "5555";
    const char *subject = (argc > 3) ? argv[3] : "test";
    long interval_ms = (argc > 4) ? strtol(argv[4], NULL, 10) : 1000;

    // Conecta al broker TCP.
    int fd = connect_tcp(host, port);
    printf("Publisher connected to %s:%s, subject='%s', every %ld ms.\n", host, port, subject, interval_ms);

    // Envía el rol "PUB" al broker.
    const char *role = "PUB\n";
    (void) send(fd, role, strlen(role), 0);

    unsigned long counter = 0;
    char header[256];
    char payload[1024];
    while (1) {
        // Crea el payload del mensaje.
        time_t now = time(NULL);
        int plen = snprintf(payload, sizeof(payload), "msg %lu at %ld", counter++, (long)now);
        // Crea la cabecera del mensaje.
        int hlen = snprintf(header, sizeof(header), "PUBLISH %s %d\n", subject, plen);
        // Envía la cabecera.
        if (send(fd, header, (size_t) hlen, 0) < 0) {
            perror("send header");
            break;
        }
        // Envía el payload.
        if (send(fd, payload, (size_t) plen, 0) < 0) {
            perror("send payload");
            break;
        }
        printf("Sent message number %lu to subject '%s'\n", counter - 1, subject);
        // Espera el intervalo de tiempo especificado.
        msleep(interval_ms);
    }
    // Cierra la conexión.
    close(fd);
    return 0;
}
