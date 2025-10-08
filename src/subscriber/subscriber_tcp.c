// subscriber_tcp.c
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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

static int read_line(int fd, char *buf, size_t max) {
    size_t n = 0;
    while (n + 1 < max) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r == 0) return 0;
        if (r < 0) return -1;
        buf[n++] = c;
        if (c == '\n') {
            buf[n] = '\0';
            return (int) n;
        }
    }
    buf[n] = '\0';
    return (int) n;
}

int main(int argc, char **argv) {
    const char *host = (argc > 1) ? argv[1] : "127.0.0.1";
    const char *port = (argc > 2) ? argv[2] : "5555";
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <host> <port> <subject> [subject2 ...]\n", argv[0]);
        fprintf(stderr, "Defaulting to subject 'test' if none provided.\n");
    }

    int fd = connect_tcp(host, port);
    printf("Subscriber connected to %s:%s\n", host, port);

    // Identify as subscriber
    const char *role = "SUB\n";
    send(fd, role, strlen(role), 0);

    // Subscribe to provided subjects (or 'test')
    if (argc < 4) {
        const char *cmd = "SUBSCRIBE test\n";
        send(fd, cmd, strlen(cmd), 0);
    } else {
        for (int i = 3; i < argc; i++) {
            char line[256];
            snprintf(line, sizeof(line), "SUBSCRIBE %s\n", argv[i]);
            send(fd, line, strlen(line), 0);
        }
    }

    // Read loop: expect frames: "MESSAGE <subject> <len>\n<payload>"
    while (1) {
        char header[512];
        int r = read_line(fd, header, sizeof(header));
        if (r <= 0) {
            printf("Connection closed.\n");
            break;
        }
        char tag[32], subject[128];
        size_t len = 0;
        if (sscanf(header, "%31s %127s %zu", tag, subject, &len) == 3 && strcmp(tag, "MESSAGE") == 0) {
            // Read payload
            char *payload = (char *) malloc(len + 1);
            size_t got = 0;
            while (got < len) {
                ssize_t n = recv(fd, payload + got, len - got, 0);
                if (n <= 0) {
                    got = 0;
                    break;
                }
                got += (size_t) n;
            }
            if (got == len) {
                payload[len] = '\0';
                printf("[%s] %s\n", subject, payload);
            } else { printf("[%s] <truncated>\n", subject); }
            free(payload);
        } else if (strncmp(header, "OK", 2) == 0) {
            // ack from broker for SUBSCRIBE
        } else if (strncmp(header, "ERR", 3) == 0) {
            fputs(header, stdout);
        } else {
            // Unknown line; print for debugging
            fputs(header, stdout);
        }
    }

    close(fd);
    return 0;
}
