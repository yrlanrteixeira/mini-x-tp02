#ifndef COMMON_H
#define COMMON_H
#include <stdint.h>
#define MAX_TEXT 141
#define PORT_DEFAULT 9000
#define TYPE_OI     0
#define TYPE_TCHAU  1
#define TYPE_MSG    2

typedef struct __attribute__((packed)) {
    uint16_t type;
    uint16_t orig_uid;
    uint16_t dest_uid;
    uint16_t text_len;
    uint8_t  text[MAX_TEXT];   // sempre com '\0' terminador
} msg_t;

int  send_full(int fd, const void *buf, size_t len);
int  recv_full(int fd, void *buf, size_t len);
int  read_msg(int fd, msg_t *m);   // converte network→host
int  write_msg(int fd, msg_t *m);  // converte host→network
void die(const char *msg);
#endif
