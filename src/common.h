/* ============================ common.h ============================ */
#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <arpa/inet.h>

#define MAX_TEXT 141

/* tipos de mensagem */
#define MSG_OI    0
#define MSG_TCHAU 1
#define MSG_MSG   2

/* estrutura do protocolo (em ordem de host) */
typedef struct __attribute__((packed)) {
    uint16_t type;
    uint16_t orig_uid;
    uint16_t dest_uid;
    uint16_t text_len;
    unsigned char text[MAX_TEXT];
} msg_t;

static inline void host_to_net_hdr(msg_t *m) {
    m->type     = htons(m->type);
    m->orig_uid = htons(m->orig_uid);
    m->dest_uid = htons(m->dest_uid);
    m->text_len = htons(m->text_len);
}
static inline void net_to_host_hdr(msg_t *m) {
    m->type     = ntohs(m->type);
    m->orig_uid = ntohs(m->orig_uid);
    m->dest_uid = ntohs(m->dest_uid);
    m->text_len = ntohs(m->text_len);
}

#endif /* COMMON_H */