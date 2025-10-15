// broker_udp.c - Broker UDP Pub/Sub
// Protocolo (datagramas):
//  SUBSCRIBE <subject>\n              (suscriptor -> broker)
//  PUBLISH   <subject> <len>\n<payload>    (publicador -> broker)
//  MESSAGE   <subject> <len>\n<payload>    (broker -> suscriptor)

#include <arpa/inet.h>     // htonl(), htons(), INADDR_ANY
#include <netinet/in.h>    // struct sockaddr_in
#include <stdio.h>         // printf(), perror()
#include <stdlib.h>        // exit(), atoi(), calloc(), free()
#include <string.h>        // memset(), memcpy(), strcmp(), strncpy(), memchr()
#include <sys/select.h>    // select(), fd_set
#include <sys/socket.h>    // socket(), bind(), recvfrom(), sendto()
#include <sys/types.h>     // tipos básicos
#include <unistd.h>        // close()

#define BROKER_PORT 5556 // Puerto por defecto para el broker UDP
#define MAX_DGRAM   2048 // Tamaño máximo del datagrama UDP

// Estructura para una entrada de suscripción. Almacena el tema (subject)
// y la dirección del suscriptor.
typedef struct SubEntry {
    char subject[128]; // Nombre del tema
    struct sockaddr_in addr; // Dirección del suscriptor
    socklen_t addrlen; // Longitud de la dirección
    struct SubEntry *next; // Puntero a la siguiente entrada en la lista
} SubEntry;

// Puntero a la cabeza de la lista enlazada de suscripciones.
static SubEntry *subs = NULL;

// Compara dos direcciones de socket para ver si son iguales.
static int addr_equal(const struct sockaddr_in *a, const struct sockaddr_in *b) {
    // Compara la familia de direcciones, la dirección IP y el puerto.
    return a->sin_family == b->sin_family && a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port;
}

// Verifica si un cliente ya está suscrito a un tema.
static int already_subscribed(const char *subject, const struct sockaddr_in *who) {
    // Recorre la lista de suscripciones.
    for (SubEntry *e = subs; e; e = e->next)
        // Si encuentra una entrada con el mismo tema y la misma dirección, retorna verdadero.
        if (strcmp(e->subject, subject) == 0 && addr_equal(&e->addr, who)) return 1;
    // Si no encuentra ninguna coincidencia, retorna falso.
    return 0;
}

// Agrega una nueva suscripción a la lista.
static void add_subscription(const char *subject, const struct sockaddr_in *who, socklen_t who_len) {
    // Si el cliente ya está suscrito, no hace nada.
    if (already_subscribed(subject, who)) return;
    // Reserva memoria para una nueva entrada de suscripción.
    SubEntry *e = (SubEntry *) calloc(1, sizeof(SubEntry));
    // Copia el nombre del tema, la dirección del cliente y la longitud de la dirección.
    strncpy(e->subject, subject, sizeof(e->subject)-1);
    e->addr = *who;
    e->addrlen = who_len;
    // Inserta la nueva entrada al principio de la lista.
    e->next = subs;
    subs = e;
}

// Envía un mensaje a todos los suscriptores de un tema.
static void fanout_message(int sock, const char *subject, const char *payload, size_t len) {
    char header[256];
    // Construye la cabecera del mensaje.
    int hlen = snprintf(header, sizeof(header), "MESSAGE %s %zu\n", subject, len);
    char buf[MAX_DGRAM];
    // Verifica si el mensaje completo (cabecera + payload) cabe en el buffer.
    if ((size_t) hlen + len > sizeof(buf)) {
        if ((size_t) hlen >= sizeof(buf)) return; // Si ni siquiera la cabecera cabe, no se puede hacer nada.
        len = sizeof(buf) - (size_t) hlen; // Trunca el payload si es necesario.
    }
    // Copia la cabecera al buffer.
    memcpy(buf, header, (size_t)hlen);
    // Si hay payload, lo copia después de la cabecera.
    if (len > 0)
        memcpy(buf+hlen, payload, len);

    // Recorre la lista de suscripciones.
    for (SubEntry *e = subs; e; e = e->next)
        // Si la suscripción es para el tema del mensaje, envía el datagrama al suscriptor.
        if (strcmp(e->subject, subject) == 0)
            (void) sendto(sock, buf, (size_t) hlen + len, 0, (struct sockaddr *) &e->addr, e->addrlen);
}

int main(int argc, char **argv) {
    // Obtiene el puerto de los argumentos de la línea de comandos, o usa el puerto por defecto.
    int port = (argc > 1) ? atoi(argv[1]) : BROKER_PORT;

    // Crea un socket UDP.
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    // Configura la dirección del broker para escuchar en cualquier interfaz.
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);
    // Asocia el socket a la dirección y puerto.
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    printf("Broker UDP started on port %d.\n", port);

    fd_set rset;
    char buf[MAX_DGRAM];
    while (1) {
        // Prepara el conjunto de descriptores de archivo para select().
        FD_ZERO(&rset);
        FD_SET(sock, &rset);
        // Espera a que lleguen datos al socket.
        if (select(sock + 1, &rset, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }
        // Si no hay datos en el socket, continúa el bucle.
        if (!FD_ISSET(sock, &rset)) continue;

        // Recibe un datagrama.
        struct sockaddr_in cli;
        socklen_t clilen = sizeof(cli);
        ssize_t n = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *) &cli, &clilen);
        if (n <= 0) continue;

        // Busca el salto de línea para separar la cabecera del payload.
        char *nl = memchr(buf, '\n', (size_t) n);
        if (!nl) continue; // Si no hay salto de línea, el datagrama está mal formado.
        size_t header_len = (size_t) (nl - buf + 1);

        // Copia la cabecera a un buffer separado.
        char header[512];
        size_t cpy = header_len < sizeof(header) - 1 ? header_len : sizeof(header) - 1;
        memcpy(header, buf, cpy);
        header[cpy] = '\0';

        char cmd[32], subject[128];
        size_t len = 0;
        // Parsea la cabecera para obtener el comando, el tema y la longitud del payload.
        if (sscanf(header, "%31s %127s %zu", cmd, subject, &len) >= 2) {
            // Si el comando es SUBSCRIBE, agrega una nueva suscripción.
            if (strcmp(cmd, "SUBSCRIBE") == 0) {
                add_subscription(subject, &cli, clilen);
                const char *ok = "OK\n";
                // Envía una confirmación al suscriptor.
                (void) sendto(sock, ok, strlen(ok), 0, (struct sockaddr *) &cli, clilen);
                // Si el comando es PUBLISH, reenvía el mensaje a los suscriptores.
            } else if (strcmp(cmd, "PUBLISH") == 0) {
                size_t payload_avail = (size_t) n - header_len;
                const char *payload = (const char *) (buf + header_len);
                if (len > payload_avail) len = payload_avail;
                // Ajusta la longitud si el payload es más corto de lo esperado.
                fanout_message(sock, subject, payload, len);
            }
        }
    }

    // Cierra el socket.
    close(sock);
    return 0;
}
