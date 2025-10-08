// broker_tcp.c
// Simple TCP Pub/Sub broker (multiple subjects) using select(); no external libs.
// Protocol (line-based control):
// 1) Client sends role line: "PUB\n" for publisher or "SUB\n" for subscriber.
// 2) Publishers send messages as: "PUBLISH <subject> <len>\n<payload-bytes>"
//    <len> is decimal bytes in payload; payload can contain any bytes (not parsed as text).
// 3) Subscribers send subscriptions as lines: "SUBSCRIBE <subject>\n" (can send multiple lines to add more subjects).
// 4) Broker forwards payloads to all subscribers of the subject as frames:
//    "MESSAGE <subject> <len>\n<payload-bytes>"
// TCP (3WHS/4WHS) is handled by the OS as we use SOCK_STREAM.

#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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
    SubjectNode *subs; // for subscribers
    char ibuf[MAX_LINE]; // control line buffer
    size_t ibuf_len;
    size_t want_payload; // for publishers when expecting payload
    char current_subject[128];
} Client;

static Client clients[MAX_CLIENTS];

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void add_subscription(Client *c, const char *subject) {
    // Prevent duplicates
    for (SubjectNode *n = c->subs; n; n = n->next) {
        if (strcmp(n->name, subject) == 0) return;
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
            // Send header then payload; ignore broken pipe errors
            if (send(c->fd, header, (size_t) hlen, 0) < 0) {
                /* ignore */
            }
            if (len > 0 && send(c->fd, payload, len, 0) < 0) {
                /* ignore */
            }
        }
    }
}

static void handle_control_line(Client *c, const char *line) {
    // Trim trailing CR/LF
    size_t L = strlen(line);
    while (L > 0 && (line[L - 1] == '\n' || line[L - 1] == '\r')) L--;
    char tmp[MAX_LINE];
    memcpy(tmp, line, L);
    tmp[L] = '\0';

    if (c->role == ROLE_UNKNOWN) {
        if (strcmp(tmp, "PUB") == 0) { c->role = ROLE_PUB; } else if (
            strcmp(tmp, "SUB") == 0) { c->role = ROLE_SUB; } else {
            const char *err = "ERR unknown role; send PUB or SUB\n";
            send(c->fd, err, strlen(err), 0);
        }
        return;
    }

    if (c->role == ROLE_SUB) {
        // Expect: SUBSCRIBE <subject>
        char cmd[32], subject[128];
        if (sscanf(tmp, "%31s %127s", cmd, subject) == 2 && strcmp(cmd, "SUBSCRIBE") == 0) {
            add_subscription(c, subject);
            const char *ok = "OK\n";
            send(c->fd, ok, strlen(ok), 0);
        } else {
            const char *err = "ERR expected: SUBSCRIBE <subject>\n";
            send(c->fd, err, strlen(err), 0);
        }
    } else if (c->role == ROLE_PUB) {
        // Expect: PUBLISH <subject> <len>
        char cmd[32], subject[128];
        size_t len = 0;
        if (sscanf(tmp, "%31s %127s %zu", cmd, subject, &len) == 3 && strcmp(cmd, "PUBLISH") == 0) {
            strncpy(c->current_subject, subject, sizeof(c->current_subject)-1);
            c->want_payload = len; // next reads will collect payload bytes
        } else {
            const char *err = "ERR expected: PUBLISH <subject> <len>\\n<payload>\n";
            send(c->fd, err, strlen(err), 0);
        }
    }
}

static void handle_readable(Client *c) {
    // If expecting payload from a publisher, read raw bytes first
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

    // Otherwise read control bytes and split on \n
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
        if (c->fd < 0) return; // closed during handler
        if (c->role == ROLE_PUB && c->want_payload > 0) break; // switch to payload mode
    }
    // Move leftover bytes to front
    size_t left = (size_t) ((c->ibuf + c->ibuf_len) - start);
    memmove(c->ibuf, start, left);
    c->ibuf_len = left;

    if (c->role == ROLE_PUB && c->want_payload > 0 && c->ibuf_len > 0) {
        size_t take = c->want_payload < c->ibuf_len ? c->want_payload : c->ibuf_len;
        broadcast_message(c->current_subject, c->ibuf, take);
        // shift remaining buffered bytes forward
        memmove(c->ibuf, c->ibuf + take, c->ibuf_len - take);
        c->ibuf_len -= take;
        c->want_payload -= take;
        if (c->want_payload == 0) c->current_subject[0] = '\0';
        // If we still need more payload, return and let the next recv() get it.
        if (c->want_payload > 0) return;
        // Otherwise, fall through so we can parse any next control lines already buffered.
    }
}

int main(int argc, char **argv) {
    int port = (argc > 1) ? atoi(argv[1]) : BROKER_PORT;

    // Ignore SIGPIPE so send() on closed sockets doesn't kill us
    signal(SIGPIPE, SIG_IGN);

    for (int i = 0; i < MAX_CLIENTS; i++) { clients[i].fd = -1; }

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) die("socket");

    int yes = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

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
        if (nready < 0) {
            if (errno == EINTR) continue;
            die("select");
        }

        if (FD_ISSET(listenfd, &rset)) {
            struct sockaddr_in cli;
            socklen_t clilen = sizeof(cli);
            int connfd = accept(listenfd, (struct sockaddr *) &cli, &clilen);
            if (connfd >= 0) {
                // Place into clients table
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
