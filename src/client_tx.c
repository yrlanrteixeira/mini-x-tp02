/* ============================ client_tx.c ============================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include "common.h"

static ssize_t recv_full(int fd, void *buf, size_t len) {
    size_t got = 0;
    while (got < len) {
        ssize_t r = recv(fd, (char*)buf + got, len - got, 0);
        if (r <= 0) return r;
        got += r;
    }
    return (ssize_t)got;
}
static ssize_t send_full(int fd, const void *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t s = send(fd, (char*)buf + sent, len - sent, 0);
        if (s <= 0) return s;
        sent += s;
    }
    return (ssize_t)sent;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <uid 1001-1999> <ip> <porta>\n", argv[0]);
        return 1;
    }
    uint16_t uid = (uint16_t)atoi(argv[1]);
    const char *ip = argv[2];
    int port = atoi(argv[3]);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in srv = {0};
    srv.sin_family = AF_INET;
    srv.sin_port   = htons(port);
    inet_pton(AF_INET, ip, &srv.sin_addr);
    if (connect(fd, (struct sockaddr*)&srv, sizeof(srv)) < 0) {
        perror("connect");
        return 1;
    }

    msg_t m = {MSG_OI, uid, 0, 0, {0}};
    host_to_net_hdr(&m);
    send_full(fd, &m, sizeof(m));
    if (recv_full(fd, &m, sizeof(m)) <= 0) {
        fprintf(stderr, "Sem resposta do servidor\n");
        return 1;
    }
    net_to_host_hdr(&m);
    if (m.type != MSG_OI) {
        fprintf(stderr, "OI rejeitado\n");
        return 1;
    }

    printf("Conectado como remetente %u. Digite '/quit' ou '<dest_uid> <texto>'\n", uid);
    char line[256];
    fd_set pfds;

    while (1) {
        FD_ZERO(&pfds);
        FD_SET(0, &pfds);
        FD_SET(fd, &pfds);
        if (select(fd + 1, &pfds, NULL, NULL, NULL) < 0) break;

        if (FD_ISSET(0, &pfds)) {
            if (!fgets(line, sizeof(line), stdin)) break;
            if (strncmp(line, "/quit", 5) == 0) {
                msg_t bye = {MSG_TCHAU, uid, 0, 0, {0}};
                host_to_net_hdr(&bye);
                send_full(fd, &bye, sizeof(bye));
                break;
            }

            char *space = strchr(line, ' ');
            if (!space) {
                puts("Formato: <dest_uid> <texto>");
                continue;
            }
            *space = '\0';
            uint16_t dst = (uint16_t)atoi(line);
            char *msg_text = space + 1;
            msg_text[strcspn(msg_text, "\n")] = '\0';

            size_t len = strlen(msg_text) + 1;
            if (len > MAX_TEXT) {
                msg_text[MAX_TEXT - 1] = '\0';
                len = MAX_TEXT;
            }

            msg_t out;
            out.type     = MSG_MSG;
            out.orig_uid = uid;
            out.dest_uid = dst;
            out.text_len = (uint16_t)len;
            memcpy(out.text, msg_text, len);

            host_to_net_hdr(&out);
            send_full(fd, &out, sizeof(out));
        }
        if (FD_ISSET(fd, &pfds)) {
            if (recv(fd, &m, sizeof(m), MSG_PEEK) <= 0) {
                puts("Servidor desconectou");
                break;
            }
        }
    }

    close(fd);
    return 0;
}
