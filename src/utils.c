#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "../include/common.h"

#define ssize_t int

// Implementações das funções definidas em common.h
int send_full(int fd, const void *buf, size_t len) {
    size_t sent = 0;
    ssize_t ret;
    
    while (sent < len) {
        ret = send(fd, (char*)buf + sent, len - sent, 0);
        if (ret <= 0) {
            if (WSAGetLastError() == WSAEINTR) continue;  // Interrompido por sinal, tentar novamente
            return -1;
        }
        sent += ret;
    }
    return sent;
}

int recv_full(int fd, void *buf, size_t len) {
    size_t received = 0;
    ssize_t ret;
    
    while (received < len) {
        ret = recv(fd, (char*)buf + received, len - received, 0);
        if (ret <= 0) {
            if (WSAGetLastError() == WSAEINTR) continue;  // Interrompido por sinal, tentar novamente
            return -1;
        }
        received += ret;
    }
    return received;
}

int read_msg(int fd, msg_t *m) {
    // Primeiro lê o cabeçalho (8 bytes)
    if (recv_full(fd, m, 8) != 8) return -1;
    
    // Converte de network para host byte order
    m->type = ntohs(m->type);
    m->orig_uid = ntohs(m->orig_uid);
    m->dest_uid = ntohs(m->dest_uid);
    m->text_len = ntohs(m->text_len);
    
    // Se tiver texto, lê o texto
    if (m->text_len > 0) {
        if (m->text_len > MAX_TEXT) m->text_len = MAX_TEXT;
        if (recv_full(fd, m->text, m->text_len) != m->text_len) return -1;
        m->text[m->text_len-1] = '\0';  // Garantir terminação correta
    }
    
    return 1;
}

int write_msg(int fd, msg_t *m) {
    msg_t net_msg = *m;
    
    // Converte de host para network byte order
    net_msg.type = htons(m->type);
    net_msg.orig_uid = htons(m->orig_uid);
    net_msg.dest_uid = htons(m->dest_uid);
    net_msg.text_len = htons(m->text_len);
    
    // Envia o cabeçalho (8 bytes)
    if (send_full(fd, &net_msg, 8) != 8) return -1;
    
    // Se tiver texto, envia o texto
    if (m->text_len > 0) {
        if (send_full(fd, m->text, m->text_len) != m->text_len) return -1;
    }
    
    return 1;
}

void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}