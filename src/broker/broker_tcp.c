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

#define BROKER_PORT 5555
#define MAX_CLIENTS  FD_SETSIZE
#define MAX_LINE     4096

typedef enum { ROLE_UNKNOWN = 0, ROLE_PUB = 1, ROLE_SUB = 2 } role_t;

typedef struct SubjectNode {
    char name[128];
    struct SubjectNode *next;
} SubjectNode;

typedef struct Client {
    int fd;
    role_t role;
    SubjectNode *subs; // para suscriptores
    char ibuf[MAX_LINE]; // buffer para líneas de control
    size_t ibuf_len;
    size_t want_payload; // bytes de payload pendientes (cuando es PUB)
    char current_subject[128];
} Client;

static Client clients[MAX_CLIENTS];

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void add_subscription(Client *c, const char *subject) {
    for (SubjectNode *n = c->subs; n; n = n->next) {
        if (strcmp(n->name, subject) == 0) return; // evitar duplicados
    }
    SubjectNode *n = (SubjectNode *) calloc(1, sizeof(SubjectNode));
    strncpy(n->name, subject, sizeof(n->name)-1);
    n->next = c->subs;
    c->subs = n;
}

static int sub_has_subject(Client *c, const char *subject) {
    for (SubjectNode *n = c->subs; n; n = n->next)
        if (strcmp(n->name, subject) == 0) return 1;
    return 0;
}

static void free_subs(SubjectNode *n) {
    while (n) {
        SubjectNode *t = n->next;
        free(n);
        n = t;
    }
}

static void close_client(Client *c) {
    if (c->fd > 0) close(c->fd);
    c->fd = -1;
    c->role = ROLE_UNKNOWN;
    c->ibuf_len = 0;
    c->want_payload = 0;
    c->current_subject[0] = '\0';
    free_subs(c->subs);
    c->subs = NULL;
}

static void broadcast_message(const char *subject, const char *payload, size_t len) {
    char header[256];
    int hlen = snprintf(header, sizeof(header), "MESSAGE %s %zu\n", subject, len);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        Client *c = &clients[i];
        if (c->fd > 0 && c->role == ROLE_SUB && sub_has_subject(c, subject)) {
            (void) send(c->fd, header, (size_t) hlen, 0); // ignorar fallos
            if (len > 0) (void) send(c->fd, payload, len, 0);
        }
    }
}

static void handle_control_line(Client *c, const char *line) {
    size_t L = strlen(line);
    while (L > 0 && (line[L - 1] == '\n' || line[L - 1] == '\r')) L--;
    char tmp[MAX_LINE];
    memcpy(tmp, line, L);
    tmp[L] = '\0';

    if (c->role == ROLE_UNKNOWN) {
        if (strcmp(tmp, "PUB") == 0) { c->role = ROLE_PUB; } else if (
            strcmp(tmp, "SUB") == 0) { c->role = ROLE_SUB; } else {
            const char *err = "ERR unknown role; send PUB or SUB\n";
            (void) send(c->fd, err, strlen(err), 0);
        }
        return;
    }

    if (c->role == ROLE_SUB) {
        char cmd[32], subject[128];
        if (sscanf(tmp, "%31s %127s", cmd, subject) == 2 && strcmp(cmd, "SUBSCRIBE") == 0) {
            add_subscription(c, subject);
            const char *ok = "OK\n";
            (void) send(c->fd, ok, strlen(ok), 0);
        } else {
            const char *err = "ERR expected: SUBSCRIBE <subject>\n";
            (void) send(c->fd, err, strlen(err), 0);
        }
    } else if (c->role == ROLE_PUB) {
        char cmd[32], subject[128];
        size_t len = 0;
        if (sscanf(tmp, "%31s %127s %zu", cmd, subject, &len) == 3 && strcmp(cmd, "PUBLISH") == 0) {
            strncpy(c->current_subject, subject, sizeof(c->current_subject)-1);
            c->want_payload = len;
        } else {
            const char *err = "ERR expected: PUBLISH <subject> <len>\\n<payload>\n";
            (void) send(c->fd, err, strlen(err), 0);
        }
    }
}

static void handle_readable(Client *c) {
    if (c->role == ROLE_PUB && c->want_payload > 0) {
        static char pbuf[65536];
        size_t toread = c->want_payload < sizeof(pbuf) ? c->want_payload : sizeof(pbuf);
        ssize_t n = recv(c->fd, pbuf, toread, 0);
        if (n <= 0) {
            close_client(c);
            return;
        }
        broadcast_message(c->current_subject, pbuf, (size_t) n);
        c->want_payload -= (size_t) n;
        if (c->want_payload == 0) c->current_subject[0] = '\0';
        return;
    }

    ssize_t n = recv(c->fd, c->ibuf + c->ibuf_len, sizeof(c->ibuf) - 1 - c->ibuf_len, 0);
    if (n <= 0) {
        close_client(c);
        return;
    }
    c->ibuf_len += (size_t) n;
    c->ibuf[c->ibuf_len] = '\0';

    char *start = c->ibuf;
    char *nl;
    while ((nl = memchr(start, '\n', (c->ibuf + c->ibuf_len) - start))) {
        size_t linelen = (size_t) (nl - start + 1);
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
