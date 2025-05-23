/* ============================ client_rx.c ============================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
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
        fprintf(stderr, "Uso: %s <uid 1-999> <ip> <porta>\n", argv[0]);
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

    printf("Conectado como exibidor %u\n", uid);
    while (1) {
        if (recv_full(fd, &m, sizeof(m)) <= 0) {
            puts("Conexão encerrada");
            break;
        }
        net_to_host_hdr(&m);
        if (m.type != MSG_MSG) continue;
        const char *scope = (m.dest_uid == 0) ? "[broadcast]" : "[privado]";
        printf("De %u %s: %s\n", m.orig_uid, scope, m.text);
    }
    close(fd);
    return 0;
}