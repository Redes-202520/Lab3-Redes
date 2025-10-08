// broker_udp.c
// Simple UDP Pub/Sub broker (multiple subjects) using select(); no external libs.
// Protocol (datagram-based):
// 1) Subscriber -> Broker:  "SUBSCRIBE <subject>\n"
// 2) Publisher  -> Broker:  "PUBLISH <subject> <len>\n<payload-bytes>"
// 3) Broker  -> Subscriber: "MESSAGE <subject> <len>\n<payload-bytes>"
// Notes: UDP has no 3-way/4-way handshake and no retransmission; this is a
// lightweight demo showing how a broker can fan out messages without TCP.


#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BROKER_PORT 5556
#define MAX_DGRAM   2048   // keep < typical MTU when including headers

typedef struct SubEntry {
    char subject[128];
    struct sockaddr_in addr;
    socklen_t addrlen;
    struct SubEntry *next;
} SubEntry;

static SubEntry *subs = NULL;

static int addr_equal(const struct sockaddr_in *a, const struct sockaddr_in *b){
    return a->sin_family==b->sin_family && a->sin_addr.s_addr==b->sin_addr.s_addr && a->sin_port==b->sin_port;
}

static int already_subscribed(const char *subject, const struct sockaddr_in *who){
    for (SubEntry *e=subs; e; e=e->next){
        if (strcmp(e->subject, subject)==0 && addr_equal(&e->addr, who)) return 1;
    }
    return 0;
}

static void add_subscription(const char *subject, const struct sockaddr_in *who, socklen_t who_len){
    if (already_subscribed(subject, who)) return;
    SubEntry *e = (SubEntry*)calloc(1, sizeof(SubEntry));
    strncpy(e->subject, subject, sizeof(e->subject)-1);
    e->addr = *who; e->addrlen = who_len;
    e->next = subs; subs = e;
}

static void fanout_message(int sock, const char *subject, const char *payload, size_t len){
    char header[256];
    int hlen = snprintf(header, sizeof(header), "MESSAGE %s %zu\n", subject, len);
    char buf[MAX_DGRAM];
    if ((size_t)(hlen) + len > sizeof(buf)){
        // truncate payload if too big for our buffer
        if ((size_t)hlen >= sizeof(buf)) return; // can't even fit header
        len = sizeof(buf) - (size_t)hlen;
    }
    memcpy(buf, header, (size_t)hlen);
    if (len>0) memcpy(buf+hlen, payload, len);

    for (SubEntry *e=subs; e; e=e->next){
        if (strcmp(e->subject, subject)==0){
            sendto(sock, buf, (size_t)hlen + len, 0, (struct sockaddr*)&e->addr, e->addrlen);
        }
    }
}

int main(int argc, char **argv){
    int port = (argc>1)? atoi(argv[1]) : BROKER_PORT;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock<0){ perror("socket"); exit(1);}

    struct sockaddr_in addr; memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_ANY); addr.sin_port = htons((uint16_t)port);
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr))<0){ perror("bind"); exit(1);}

    printf("Broker UDP started on port %d.\n", port);

    fd_set rset; char buf[MAX_DGRAM];
    while (1){
        FD_ZERO(&rset); FD_SET(sock, &rset);
        if (select(sock+1, &rset, NULL, NULL, NULL) < 0){ if (errno==EINTR) continue; perror("select"); break; }
        if (!FD_ISSET(sock, &rset)) continue;

        struct sockaddr_in cli; socklen_t clilen = sizeof(cli);
        ssize_t n = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&cli, &clilen);
        if (n<=0) continue;

        // Expect a header line optionally followed by payload
        // Find first \n as delimiter
        char *nl = memchr(buf, '\n', (size_t)n);
        if (!nl){ continue; } // malformed; ignore
        size_t header_len = (size_t)(nl - buf + 1);

        // Build a zero-terminated header copy to parse easily
        char header[512];
        size_t cpy = header_len < sizeof(header)-1 ? header_len : sizeof(header)-1;
        memcpy(header, buf, cpy); header[cpy] = '\0';

        // Parse
        char cmd[32], subject[128]; size_t len=0;
        if (sscanf(header, "%31s %127s %zu", cmd, subject, &len) >= 2){
            if (strcmp(cmd, "SUBSCRIBE")==0){
                add_subscription(subject, &cli, clilen);
                const char *ok = "OK\n";
                sendto(sock, ok, strlen(ok), 0, (struct sockaddr*)&cli, clilen);
            } else if (strcmp(cmd, "PUBLISH")==0){
                // Payload should follow after header delimiter
                size_t payload_avail = (size_t)n - header_len;
                const char *payload = (const char*)(buf + header_len);
                if (len > payload_avail) len = payload_avail; // clip if short datagram
                fanout_message(sock, subject, payload, len);
            }
        }
    }

    close(sock);
    return 0;
}