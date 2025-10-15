// broker_tcp.c — Broker TCP Pub/Sub
// Protocolo (líneas de control):
//  1) Cliente envía rol: "PUB\n" o "SUB\n".
//  2) Publicadores: "PUBLISH <subject> <len>\n<payload>"
//  3) Suscriptores: "SUBSCRIBE <subject>\n" (pueden enviar varias).
//  4) Broker reenvía a suscriptores del tema:
//     "MESSAGE <subject> <len>\n<payload>"
// TCP hace 3 way handshake/4 way handshake en el kernel, solo usamos SOCK_STREAM.

#include <arpa/inet.h>     // htonl(), htons(), INADDR_ANY
#include <netinet/in.h>    // struct sockaddr_in
#include <signal.h>        // signal(), SIGPIPE, SIG_IGN
#include <stdio.h>         // printf(), perror()
#include <stdlib.h>        // exit(), EXIT_FAILURE, calloc(), free(), atoi()
#include <string.h>        // memset(), memcpy(), strcmp(), strncmp(), strncpy()
#include <sys/select.h>    // select(), fd_set y macros FD_*
#include <sys/socket.h>    // socket(), bind(), listen(), accept(), send(), recv()
#include <sys/types.h>     // tipos básicos de sockets
#include <unistd.h>        // close()

#define BROKER_PORT 5555 // puerto TCP
#define MAX_CLIENTS  FD_SETSIZE // limitar la cantidad de clientes conectados al máximo que soporta select() (File Descriptor Set SIZE)

#define MAX_LINE 4096 // tamaño máximo de línea de control en bytes

typedef enum { ROLE_UNKNOWN = 0, ROLE_PUB = 1, ROLE_SUB = 2 } role_t; // roles de cliente

// Lista enlazada de temas (subjects) a los que está suscrito un cliente
typedef struct SubjectNode {
    char name[128];
    struct SubjectNode *next;
} SubjectNode;

// Estructura para cada cliente conectado
typedef struct Client {
    int fd; // descriptor de socket
    role_t role; // rol: PUB, SUB o UNKNOWN
    SubjectNode *subs; // para suscriptores
    char ibuf[MAX_LINE]; // buffer para líneas de control
    size_t ibuf_len; // bytes actualmente en ibuf
    size_t want_payload; // bytes de payload pendientes (cuando es PUB)
    char current_subject[128]; // tema actual (cuando es PUB)
} Client;

static Client clients[MAX_CLIENTS]; // array de clientes

// Funciones auxiliares

// Imprimir mensaje de error y salir
static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Agregar un tema a la lista de suscripciones del cliente (evitar duplicados)
static void add_subscription(Client *c, const char *subject) {
    // verificar si ya está suscrito
    for (SubjectNode *n = c->subs; n; n = n->next) {
        if (strcmp(n->name, subject) == 0) return; // evitar duplicados
    }
    // agregar al inicio de la lista
    SubjectNode *n = (SubjectNode *) calloc(1, sizeof(SubjectNode)); // inicializado a 0
    strncpy(n->name, subject, sizeof(n->name)-1); // asegurar null-terminación
    n->next = c->subs; // insertar al inicio
    c->subs = n; // actualizar cabeza de lista
}

// Verificar si el cliente está suscrito a un tema
static int sub_has_subject(Client *c, const char *subject) {
    // buscar en la lista
    for (SubjectNode *n = c->subs; n; n = n->next)
        // strcmp devuelve 0 si son iguales (strcmp es una comparación de strings)
        if (strcmp(n->name, subject) == 0) return 1;
    return 0;
}

// Liberar la lista de temas
static void free_subs(SubjectNode *n) {
    // recorrer y liberar cada nodo
    while (n) {
        SubjectNode *t = n->next; // guardar siguiente
        free(n); // liberar nodo actual
        n = t; // avanzar
    }
}

// Liberar todos los recursos del cliente y cerrar su socket
static void close_client(Client *c) {
    if (c->fd > 0) close(c->fd); // cerrar socket si está abierto
    c->fd = -1; // marcar como cerrado
    c->role = ROLE_UNKNOWN; // resetear rol
    c->ibuf_len = 0; // resetear buffer de entrada
    c->want_payload = 0; // resetear contador de payload pendiente
    c->current_subject[0] = '\0'; // resetear tema actual
    free_subs(c->subs); // liberar lista de temas
    c->subs = NULL; // resetear puntero a lista
}

// Enviar un mensaje a todos los suscriptores del tema
static void broadcast_message(const char *subject, const char *payload, size_t len) {
    char header[256]; // cabecera del mensaje (string)
    int hlen = snprintf(header, sizeof(header), "MESSAGE %s %zu\n", subject, len); // construir cabecera
    // recorrer todos los clientes
    for (int i = 0; i < MAX_CLIENTS; i++) {
        Client *c = &clients[i]; // puntero al cliente actual
        // enviar solo si es suscriptor y está suscrito al tema
        if (c->fd > 0 && c->role == ROLE_SUB && sub_has_subject(c, subject)) {
            // enviar cabecera y payload
            (void) send(c->fd, header, (size_t) hlen, 0);
            if (len > 0) (void) send(c->fd, payload, len, 0);
        }
    }
}

// Manejar una línea de control recibida del cliente
static void handle_control_line(Client *c, const char *line) {
    size_t L = strlen(line); // longitud de la línea (size_t es un entero sin signo)
    // recortar saltos de línea al final
    while (L > 0 && (line[L - 1] == '\n' || line[L - 1] == '\r')) L--;
    char tmp[MAX_LINE]; // buffer temporal para la línea sin saltos (string)
    memcpy(tmp, line, L); // copiar línea
    tmp[L] = '\0'; // asegurar null-terminación

    // Si el rol es desconocido, esperar "PUB" o "SUB"
    if (c->role == ROLE_UNKNOWN) {
        if (strcmp(tmp, "PUB") == 0) { // rol publicador
            c->role = ROLE_PUB; // inicializar estado de publicador
        } else if (strcmp(tmp, "SUB") == 0) { // rol suscriptor
            c->role = ROLE_SUB; // inicializar estado de suscriptor
        }
        else { // línea inválida
            const char *err = "ERR unknown role; send PUB or SUB\n";
            (void) send(c->fd, err, strlen(err), 0); // notificar error
        }
        return;
    }

    if (c->role == ROLE_SUB) { // manejar línea de suscriptor
        char cmd[32]; // comando (string)
        char subject[128]; // tema (string)
        if (sscanf(tmp, "%31s %127s", cmd, subject) == 2 && strcmp(cmd, "SUBSCRIBE") == 0) { // parsear línea con sscanf
            add_subscription(c, subject); // agregar tema a la lista
            const char *ok = "OK\n"; // confirmar suscripción
            (void) send(c->fd, ok, strlen(ok), 0); // enviar ACK
        } else { // línea inválida
            const char *err = "ERR expected: SUBSCRIBE <subject>\n"; //
            (void) send(c->fd, err, strlen(err), 0); // notificar error
        }
    } else if (c->role == ROLE_PUB) { // manejar línea de publicador
        char cmd[32]; // comando (string)
        char subject[128]; // tema (string)
        size_t len = 0; // longitud del payload (size_t es un entero sin signo)
        if (sscanf(tmp, "%31s %127s %zu", cmd, subject, &len) == 3 && strcmp(cmd, "PUBLISH") == 0) { // parsear línea
            strncpy(c->current_subject, subject, sizeof(c->current_subject)-1); // guardar tema actual
            c->want_payload = len; // establecer bytes de payload pendientes
        } else { // línea inválida
            const char *err = "ERR expected: PUBLISH <subject> <len>\\n<payload>\n"; // mensaje de error
            (void) send(c->fd, err, strlen(err), 0); // notificar error
        }
    }
}

// Manejar datos legibles en el socket del cliente
static void handle_readable(Client *c) { // Client es un puntero a la estructura del cliente
    if (c->role == ROLE_PUB && c->want_payload > 0) { // modo payload
        static char pbuf[65536]; // buffer temporal para payload (static para no usar stack)
        size_t toread = c->want_payload < sizeof(pbuf) ? c->want_payload : sizeof(pbuf); // bytes a leer
        ssize_t n = recv(c->fd, pbuf, toread, 0); // leer del socket
        if (n <= 0) { // error o conexión cerrada
            close_client(c);
            return;
        }
        broadcast_message(c->current_subject, pbuf, (size_t) n); // reenviar a suscriptores
        c->want_payload -= (size_t) n; // actualizar bytes pendientes
        if (c->want_payload == 0) c->current_subject[0] = '\0'; // resetear tema actual
        return;
    }

    ssize_t n = recv(c->fd, c->ibuf + c->ibuf_len, sizeof(c->ibuf) - 1 - c->ibuf_len, 0); // leer línea de control
    if (n <= 0) { // error o conexión cerrada
        close_client(c);
        return;
    }
    c->ibuf_len += (size_t) n; // actualizar longitud del buffer
    c->ibuf[c->ibuf_len] = '\0'; // asegurar null-terminación

    char *start = c->ibuf; // puntero al inicio del buffer
    char *nl; // puntero al salto de línea
    // procesar todas las líneas completas en el buffer
    while ((nl = memchr(start, '\n', (c->ibuf + c->ibuf_len) - start))) {
        size_t linelen = (size_t) (nl - start + 1); // longitud de la línea incluyendo '\n'
        char line[MAX_LINE];
        memcpy(line, start, linelen);
        line[linelen] = '\0';
        handle_control_line(c, line);
        start = nl + 1;
        if (c->fd < 0) return;
        if (c->role == ROLE_PUB && c->want_payload > 0) break; // pasa a modo payload
    }
    size_t left = (size_t) ((c->ibuf + c->ibuf_len) - start);
    memmove(c->ibuf, start, left);
    c->ibuf_len = left;

    if (c->role == ROLE_PUB && c->want_payload > 0 && c->ibuf_len > 0) {
        size_t take = c->want_payload < c->ibuf_len ? c->want_payload : c->ibuf_len;
        broadcast_message(c->current_subject, c->ibuf, take);
        memmove(c->ibuf, c->ibuf + take, c->ibuf_len - take);
        c->ibuf_len -= take;
        c->want_payload -= take;
        if (c->want_payload == 0) c->current_subject[0] = '\0';
        if (c->want_payload > 0) return;
    }
}

int main(int argc, char **argv) {
    int port = (argc > 1) ? atoi(argv[1]) : BROKER_PORT;

    // Evitar SIGPIPE al enviar a sockets cerrados
    signal(SIGPIPE, SIG_IGN);

    for (int i = 0; i < MAX_CLIENTS; i++) { clients[i].fd = -1; }

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) die("socket");

    int yes = 1;
    (void) setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);
    if (bind(listenfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) die("bind");
    if (listen(listenfd, 128) < 0) die("listen");

    printf("Broker TCP started on port %d.\n", port);

    fd_set rset;
    int maxfd = listenfd;
    while (1) {
        FD_ZERO(&rset);
        FD_SET(listenfd, &rset);
        for (int i = 0; i < MAX_CLIENTS; i++)
            if (clients[i].fd > 0)
                FD_SET(clients[i].fd, &rset);
        int nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (nready < 0) die("select");

        if (FD_ISSET(listenfd, &rset)) {
            struct sockaddr_in cli;
            socklen_t clilen = sizeof(cli);
            int connfd = accept(listenfd, (struct sockaddr *) &cli, &clilen);
            if (connfd >= 0) {
                int placed = 0;
                for (int i = 0; i < MAX_CLIENTS; i++)
                    if (clients[i].fd < 0) {
                        clients[i].fd = connfd;
                        clients[i].role = ROLE_UNKNOWN;
                        clients[i].ibuf_len = 0;
                        clients[i].want_payload = 0;
                        clients[i].subs = NULL;
                        clients[i].current_subject[0] = '\0';
                        placed = 1;
                        break;
                    }
                if (!placed) { close(connfd); }
                if (connfd > maxfd) maxfd = connfd;
            }
            if (--nready <= 0) continue;
        }
        for (int i = 0; i < MAX_CLIENTS; i++) {
            Client *c = &clients[i];
            if (c->fd > 0 && FD_ISSET(c->fd, &rset)) {
                handle_readable(c);
            }
        }
    }
}

/*
===============================================================================
Explicación detallada de las librerías usadas (y dónde se usan)
-------------------------------------------------------------------------------
<arpa/inet.h>
  - Proporciona utilidades de conversión de byte-order y constantes de red.
  - Uso en main(): htonl(), htons(), INADDR_ANY para configurar la IP:puerto del
    socket de escucha.

<netinet/in.h>
  - Define struct sockaddr_in y valores AF_INET.
  - Uso en main(): variables 'addr', 'cli'; y en accept().

<signal.h>
  - Manejo de señales del proceso.
  - Uso en main(): signal(SIGPIPE, SIG_IGN) para que un send() a un peer cerrado
    no termine el proceso con SIGPIPE.

<stdio.h>
  - E/S estándar y diagnósticos.
  - Uso en printf() (mensajes de estado) y perror() (errores fatales en die()).

<stdlib.h>
  - Utilidades generales: exit(), calloc(), free(), atoi().
  - Uso: die() llama exit(); add_subscription() usa calloc(); free_subs() usa
    free(); lectura de puerto con atoi().

<string.h>
  - Manipulación de memoria/cadenas.
  - Uso: memset(), memcpy(), strcmp()/strncmp(), strncpy(), memchr(), memmove().

<sys/select.h>
  - API de multiplexación select() y macros FD_SET, FD_ZERO, etc.
  - Uso en el bucle principal para esperar actividad en listenfd y clientes.

<sys/socket.h>
  - API de sockets POSIX.
  - Uso: socket(), bind(), listen(), accept(), send(), recv(), setsockopt().

<sys/types.h>
  - Tipos base para sockets (socklen_t, etc.).
  - Uso implícito por llamadas de sockets.

<unistd.h>
  - Llamadas POSIX a bajo nivel.
  - Uso: close() para cerrar descriptores.
===============================================================================
*/
