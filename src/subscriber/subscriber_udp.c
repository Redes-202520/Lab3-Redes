// subscriber_udp.c

#include <arpa/inet.h>      // htonl(), htons() si se necesitaran; struct in_addr
#include <netdb.h>          // getaddrinfo(), freeaddrinfo(), gai_strerror()
#include <netinet/in.h>     // struct sockaddr_in
#include <stdio.h>          // printf(), fprintf()
#include <stdlib.h>         // exit()
#include <string.h>         // memset(), memcpy(), snprintf(), memchr(), strcmp(), sscanf()
#include <sys/socket.h>     // socket(), bind(), sendto(), recvfrom()
#include <sys/types.h>      // tipos básicos
#include <unistd.h>         // close()

int main(int argc, char **argv) {
    // Obtiene los parámetros de la línea de comandos o usa valores por defecto.
    const char *host = (argc > 1) ? argv[1] : "127.0.0.1";
    const char *port = (argc > 2) ? argv[2] : "5556"; // puerto UDP del broker

    // Crea un socket UDP.
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    // Configura la dirección local para escuchar en cualquier interfaz y en un puerto efímero.
    struct sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(0);
    if (bind(sock, (struct sockaddr *) &local, sizeof(local)) < 0) {
        perror("bind");
        return 1;
    }

    // Resuelve la dirección del broker.
    struct addrinfo hints, *res;
    int rc;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if ((rc = getaddrinfo(host, port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        return 1;
    }

    printf("Subscriber connected to %s:%s\n", host, port);

    // Envía los mensajes de suscripción al broker.
    if (argc < 4) {
        const char *line = "SUBSCRIBE test\n";
        (void) sendto(sock, line, strlen(line), 0, res->ai_addr, res->ai_addrlen);
    } else {
        for (int i = 3; i < argc; i++) {
            char line[256];
            snprintf(line, sizeof(line), "SUBSCRIBE %s\n", argv[i]);
            (void) sendto(sock, line, strlen(line), 0, res->ai_addr, res->ai_addrlen);
        }
    }

    char buf[2048];
    while (1) {
        // Espera a recibir un datagrama.
        struct sockaddr_in from;
        socklen_t fromlen = sizeof(from);
        ssize_t n = recvfrom(sock, buf, sizeof(buf) - 1, 0, (struct sockaddr *) &from, &fromlen);
        if (n <= 0) continue;
        buf[n] = '\0';
        // Busca el salto de línea para separar la cabecera del payload.
        char *nl = memchr(buf, '\n', (size_t) n);
        if (!nl) continue;
        size_t header_len = (size_t) (nl - buf + 1);
        char header[512];
        // Copia la cabecera a un buffer separado.
        size_t cpy = header_len < sizeof(header) - 1 ? header_len : sizeof(header) - 1;
        memcpy(header, buf, cpy);
        header[cpy] = '\0';
        char tag[32], subject[128];
        size_t len = 0;
        // Parsea la cabecera.
        if (sscanf(header, "%31s %127s %zu", tag, subject, &len) == 3 && strcmp(tag, "MESSAGE") == 0) {
            size_t avail = (size_t) n - header_len;
            if (len > avail) len = avail;
            char save = ((char *) buf)[header_len + len];
            ((char *) buf)[header_len + len] = '\0';
            // Imprime el mensaje.
            printf("[%s] %s\n", subject, (char *) buf + header_len);
            ((char *) buf)[header_len + len] = save;
        }
    }

    freeaddrinfo(res);
    close(sock);
    return 0;
}
