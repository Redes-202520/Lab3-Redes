// publisher_tcp.c

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
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

    // Identify as publisher
    const char *role = "PUB\n";
    send(fd, role, strlen(role), 0);

    unsigned long counter = 0;
    char header[256];
    char payload[1024];
    while (1) {
        // Build payload
        time_t now = time(NULL);
        int plen = snprintf(payload, sizeof(payload), "msg %lu at %ld", counter++, (long)now);
        int hlen = snprintf(header, sizeof(header), "PUBLISH %s %d\n", subject, plen);
        // Send header then payload
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
