/* ============================ server.c ============================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "common.h"

#define MAX_CLIENTS    10
#define DEFAULT_PORT   12345
#define INFO_INTERVAL  60

typedef struct {
    int fd;
    uint16_t uid;
} client_t;

static client_t displays[MAX_CLIENTS];
static client_t senders[MAX_CLIENTS];
static int num_displays = 0, num_senders = 0;
static time_t start_ts;
static volatile sig_atomic_t tick_flag = 0;

static void alarm_handler(int sig) {
    (void)sig;
    tick_flag = 1;
    alarm(INFO_INTERVAL);
}

static void add_client(client_t *list, int *count, int fd, uint16_t uid) {
    if (*count < MAX_CLIENTS) {
        list[*count].fd  = fd;
        list[*count].uid = uid;
        (*count)++;
    }
}
static void remove_client(client_t *list, int *count, int idx) {
    close(list[idx].fd);
    list[idx] = list[*count - 1];
    (*count)--;
}
static int find_client_idx(client_t *list, int count, uint16_t uid) {
    for (int i = 0; i < count; i++) {
        if (list[i].uid == uid) return i;
    }
    return -1;
}

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
        ssize_t s = send(fd, (const char*)buf + sent, len - sent, 0);
        if (s <= 0) return s;
        sent += s;
    }
    return (ssize_t)sent;
}

/* encaminha msg_t em network-byte-order */
static void forward_msg(const msg_t *m) {
    uint16_t dest = ntohs(m->dest_uid);
    if (dest == 0) {
        for (int i = 0; i < num_displays; i++) {
            send_full(displays[i].fd, m, sizeof(*m));
        }
    } else {
        int idx = find_client_idx(displays, num_displays, dest);
        if (idx != -1) {
            send_full(displays[idx].fd, m, sizeof(*m));
        }
    }
}

int main(int argc, char *argv[]) {
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); exit(1); }
    int yes = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in srv = {0};
    srv.sin_family      = AF_INET;
    srv.sin_addr.s_addr = INADDR_ANY;
    srv.sin_port        = htons(port);
    if (bind(listen_fd, (struct sockaddr*)&srv, sizeof(srv)) < 0) {
        perror("bind"); exit(1);
    }
    listen(listen_fd, 16);

    signal(SIGALRM, alarm_handler);
    alarm(INFO_INTERVAL);
    start_ts = time(NULL);

    printf("Servidor ouvindo na porta %d...\n", port);

    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(listen_fd, &rfds);
        int maxfd = listen_fd;
        for (int i = 0; i < num_displays; i++) {
            FD_SET(displays[i].fd, &rfds);
            if (displays[i].fd > maxfd) maxfd = displays[i].fd;
        }
        for (int i = 0; i < num_senders; i++) {
            FD_SET(senders[i].fd, &rfds);
            if (senders[i].fd > maxfd) maxfd = senders[i].fd;
        }

        int ret = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (ret < 0 && errno == EINTR) {
            if (tick_flag) {
                tick_flag = 0;
                /* broadcast de info */
                char buf[128];
                time_t now = time(NULL);
                int up = (int)(now - start_ts);
                snprintf(buf, sizeof(buf), "Servidor mini-X · displays:%d · uptime:%ds", num_displays, up);
                msg_t info;
                info.type     = MSG_MSG;
                info.orig_uid = 0;
                info.dest_uid = 0;
                info.text_len = (uint16_t)(strlen(buf) + 1);
                memcpy(info.text, buf, info.text_len);
                host_to_net_hdr(&info);
                forward_msg(&info);
            }
            continue;
        } else if (ret < 0) {
            perror("select");
            break;
        }

        /* nova conexão */
        if (FD_ISSET(listen_fd, &rfds)) {
            struct sockaddr_in cli;
            socklen_t clen = sizeof(cli);
            int cfd = accept(listen_fd, (struct sockaddr*)&cli, &clen);
            if (cfd >= 0) {
                msg_t m;
                if (recv_full(cfd, &m, sizeof(m)) > 0) {
                    net_to_host_hdr(&m);
                    if (m.type == MSG_OI) {
                        uint16_t uid = m.orig_uid;
                        int is_disp = (uid > 0 && uid < 1000);
                        int is_send = (uid >= 1001 && uid <= 1999);
                        if (is_disp && num_displays < MAX_CLIENTS && find_client_idx(displays, num_displays, uid) < 0)
                            add_client(displays, &num_displays, cfd, uid);
                        else if (is_send && num_senders < MAX_CLIENTS && find_client_idx(senders, num_senders, uid) < 0)
                            add_client(senders, &num_senders, cfd, uid);
                        else {
                            close(cfd);
                            continue;
                        }
                        /* eco */
                        host_to_net_hdr(&m);
                        send_full(cfd, &m, sizeof(m));
                    } else close(cfd);
                } else close(cfd);
            }
        }

        /* processa mensagens de senders */
        for (int i = 0; i < num_senders; i++) {
            if (FD_ISSET(senders[i].fd, &rfds)) {
                msg_t m;
                ssize_t r = recv_full(senders[i].fd, &m, sizeof(m));
                if (r <= 0) {
                    remove_client(senders, &num_senders, i);
                    i--;
                    continue;
                }
                net_to_host_hdr(&m);
                if (m.type == MSG_TCHAU) {
                    remove_client(senders, &num_senders, i);
                    i--;
                } else if (m.type == MSG_MSG && m.orig_uid == senders[i].uid) {
                    msg_t m_net = m;
                    host_to_net_hdr(&m_net);
                    forward_msg(&m_net);
                }
            }
        }

        /* detecta exibição desconectada */
        for (int i = 0; i < num_displays; i++) {
            if (FD_ISSET(displays[i].fd, &rfds)) {
                char tmp;
                if (recv(displays[i].fd, &tmp, 1, MSG_PEEK) <= 0) {
                    remove_client(displays, &num_displays, i);
                    i--;
                }
            }
        }
    }
    return 0;
}