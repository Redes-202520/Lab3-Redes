// publisher_udp.c

#include <netdb.h>          // getaddrinfo(), freeaddrinfo(), gai_strerror()
#include <stdio.h>          // printf(), fprintf(), perror()
#include <stdlib.h>         // strtol()
#include <string.h>         // memset(), strlen(), snprintf(), memcpy()
#include <sys/socket.h>     // socket(), sendto()
#include <sys/types.h>      // tipos de socket
#include <time.h>           // time(), nanosleep(), struct timespec
#include <unistd.h>         // close()

// Función para pausar la ejecución durante un número de milisegundos.
static void msleep(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts,NULL);
}

int main(int argc, char **argv) {
    // Obtiene los parámetros de la línea de comandos o usa valores por defecto.
    const char *host = (argc > 1) ? argv[1] : "127.0.0.1";
    const char *port = (argc > 2) ? argv[2] : "5556"; // puerto UDP
    const char *subject = (argc > 3) ? argv[3] : "test";
    long interval_ms = (argc > 4) ? strtol(argv[4], NULL, 10) : 1000;

    struct addrinfo hints, *res;
    int rc;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP
    // Resuelve la dirección del broker.
    if ((rc = getaddrinfo(host, port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        return 1;
    }

    // Crea un socket UDP.
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        perror("socket");
        freeaddrinfo(res);
        return 1;
    }

    printf("Publisher UDP connected to %s:%s, subject='%s', every %ld ms.\n", host, port, subject, interval_ms);

    unsigned long counter = 0;
    char header[256];
    char payload[1200];
    char frame[1600];

    while (1) {
        // Crea el payload del mensaje.
        time_t now = time(NULL);
        int plen = snprintf(payload, sizeof(payload), "msg %lu at %ld", counter++, (long)now);
        // Crea la cabecera del mensaje.
        int hlen = snprintf(header, sizeof(header), "PUBLISH %s %d\n", subject, plen);
        // Calcula el tamaño total del datagrama.
        size_t total = (size_t) hlen + (size_t) plen;
        if (total > sizeof(frame)) total = sizeof(frame);
        // Copia la cabecera y el payload al buffer del datagrama.
        memcpy(frame, header, (size_t)hlen);
        memcpy(frame+hlen, payload, (size_t)(total - (size_t)hlen));
        // Envía el datagrama al broker.
        (void) sendto(sock, frame, total, 0, res->ai_addr, res->ai_addrlen);
        printf("Sent message number %lu to subject '%s'\n", counter - 1, subject);
        // Espera el intervalo de tiempo especificado.
        msleep(interval_ms);
    }

    // Cierra el socket.
    close(sock);
    freeaddrinfo(res);
    return 0;
}
