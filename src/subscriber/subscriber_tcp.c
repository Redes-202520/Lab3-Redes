// subscriber_tcp.c

#include <netdb.h>          // getaddrinfo(), freeaddrinfo(), gai_strerror()
#include <stdio.h>          // printf(), perror(), fputs()
#include <stdlib.h>         // exit(), malloc(), free()
#include <string.h>         // memset(), strncmp(), strcmp(), sscanf(), snprintf()
#include <sys/socket.h>     // socket(), connect(), send(), recv()
#include <sys/types.h>      // tipos de socket
#include <unistd.h>         // close()

// Función para conectar a un servidor TCP.
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

// Función para leer una línea del socket.
static int read_line(int fd, char *buf, size_t max) {
    size_t n = 0;
    while (n + 1 < max) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0); // Lee un byte a la vez.
        if (r == 0) return 0; // Conexión cerrada.
        if (r < 0) return -1; // Error.
        buf[n++] = c;
        if (c == '\n') {
            // Si encuentra un salto de línea, termina la línea.
            buf[n] = '\0';
            return (int) n;
        }
    }
    buf[n] = '\0';
    return (int) n;
}

int main(int argc, char **argv) {
    // Obtiene los parámetros de la línea de comandos o usa valores por defecto.
    const char *host = (argc > 1) ? argv[1] : "127.0.0.1";
    const char *port = (argc > 2) ? argv[2] : "5555";

    // Conecta al broker TCP.
    int fd = connect_tcp(host, port);
    printf("Subscriber connected to %s:%s\n", host, port);

    // Envía el rol "SUB" al broker.
    const char *role = "SUB\n";
    (void) send(fd, role, strlen(role), 0);

    // Se suscribe a los temas especificados en la línea de comandos.
    if (argc < 4) {
        const char *cmd = "SUBSCRIBE test\n"; // Si no se especifican temas, se suscribe a "test".
        (void) send(fd, cmd, strlen(cmd), 0);
    } else {
        for (int i = 3; i < argc; i++) {
            char line[256];
            snprintf(line, sizeof(line), "SUBSCRIBE %s\n", argv[i]);
            (void) send(fd, line, strlen(line), 0);
        }
    }

    while (1) {
        char header[512];
        // Lee la cabecera del mensaje.
        int r = read_line(fd, header, sizeof(header));
        if (r <= 0) {
            printf("Connection closed.\n");
            break;
        }
        char tag[32], subject[128];
        size_t len = 0;
        // Parsea la cabecera para obtener el tag, el tema y la longitud del payload.
        if (sscanf(header, "%31s %127s %zu", tag, subject, &len) == 3 && strcmp(tag, "MESSAGE") == 0) {
            // Reserva memoria para el payload.
            char *payload = (char *) malloc(len + 1);
            size_t got = 0;
            // Lee el payload del socket.
            while (got < len) {
                ssize_t n = recv(fd, payload + got, len - got, 0);
                if (n <= 0) {
                    got = 0;
                    break;
                }
                got += (size_t) n;
            }
            // Si se recibió el payload completo, lo imprime.
            if (got == len) {
                payload[len] = '\0';
                printf("[%s] %s\n", subject, payload);
            } else { printf("[%s] <truncated>\n", subject); }
            free(payload);
        } else if (strncmp(header, "OK", 2) == 0) {
            // Ignora los mensajes "OK" del broker.
        } else if (strncmp(header, "ERR", 3) == 0) {
            // Imprime los mensajes de error del broker.
            fputs(header, stdout);
        } else {
            // Imprime cualquier otro mensaje para depuración.
            fputs(header, stdout);
        }
    }

    // Cierra la conexión.
    close(fd);
    return 0;
}
