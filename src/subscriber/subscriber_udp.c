// subscriber_udp.c
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

int main(int argc, char **argv){
    const char *host = (argc>1)? argv[1] : "127.0.0.1";
    const char *port = (argc>2)? argv[2] : "5556"; // default UDP port (must match broker)

    // Create a UDP socket bound to an ephemeral local port
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock<0){ perror("socket"); return 1; }

    struct sockaddr_in local; memset(&local,0,sizeof(local));
    local.sin_family = AF_INET; local.sin_addr.s_addr = htonl(INADDR_ANY); local.sin_port = htons(0);
    if (bind(sock, (struct sockaddr*)&local, sizeof(local))<0){ perror("bind"); return 1; }

    // Resolve broker addr
    struct addrinfo hints, *res; int rc;
    memset(&hints,0,sizeof(hints)); hints.ai_family=AF_INET; hints.ai_socktype=SOCK_DGRAM;
    if ((rc=getaddrinfo(host, port, &hints, &res))!=0){ fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc)); return 1; }

    printf("Subscriber connected to %s:%s\n", host, port);

    // Register subscriptions
    if (argc<4){
        const char *line = "SUBSCRIBE test\n";
        sendto(sock, line, strlen(line), 0, res->ai_addr, res->ai_addrlen);
    } else {
        for (int i=3; i<argc; i++){
            char line[256];
            snprintf(line, sizeof(line), "SUBSCRIBE %s\n", argv[i]);
            sendto(sock, line, strlen(line), 0, res->ai_addr, res->ai_addrlen);
        }
    }

    // Receive loop
    char buf[2048];
    while (1){
        struct sockaddr_in from; socklen_t fromlen = sizeof(from);
        ssize_t n = recvfrom(sock, buf, sizeof(buf)-1, 0, (struct sockaddr*)&from, &fromlen);
        if (n<=0) continue;
        buf[n]='\0';
        // Expect: "MESSAGE <subject> <len>\n<payload>"
        char *nl = memchr(buf, '\n', (size_t)n);
        if (!nl) continue;
        size_t header_len = (size_t)(nl - buf + 1);
        char header[512]; size_t cpy = header_len < sizeof(header)-1 ? header_len : sizeof(header)-1; memcpy(header, buf, cpy); header[cpy]='\0';
        char tag[32], subject[128]; size_t len=0;
        if (sscanf(header, "%31s %127s %zu", tag, subject, &len)==3 && strcmp(tag, "MESSAGE")==0){
            size_t avail = (size_t)n - header_len; if (len>avail) len = avail;
            char save = ((char*)buf)[header_len + len];
            ((char*)buf)[header_len + len] = '\0';
            printf("[%s] %s\n", subject, (char*)buf + header_len);
            ((char*)buf)[header_len + len] = save;
        }
    }

    freeaddrinfo(res);
    close(sock);
    return 0;
}
