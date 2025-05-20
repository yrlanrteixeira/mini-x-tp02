#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <time.h>
#include "../include/common.h"

// Windows não tem alarm(), então precisamos implementar nossa própria versão
#define EAGAIN WSAEWOULDBLOCK
#define EINTR  WSAEINTR
#define ssize_t int

// Linkar com a biblioteca Winsock
#pragma comment(lib, "Ws2_32.lib")

#define MAX_CLIENTS 20  // 10 de exibição e 10 de envio
#define SERVER_NAME "Mini-X Server v1.0"

// Tipos de cliente
#define CLIENT_NONE      0  // Slot vazio
#define CLIENT_DISPLAY   1  // Cliente de exibição (1-999)
#define CLIENT_SENDER    2  // Cliente de envio (1001-1999)

typedef struct {
    int socket;             // Socket do cliente
    int uid;                // ID do cliente
    int type;               // Tipo do cliente (CLIENT_DISPLAY ou CLIENT_SENDER)
    time_t connect_time;    // Tempo de conexão
} client_t;

client_t clients[MAX_CLIENTS];
int server_socket;
time_t server_start_time;
volatile int timer_expired = 0;

// Função de thread para simular o alarm() no Windows
DWORD WINAPI timerThreadProc(LPVOID lpParam) {
    while (1) {
        Sleep(60000); // 60 segundos
        timer_expired = 1;
    }
    return 0;
}

// Funções auxiliares para procurar clientes
int find_client_by_socket(int socket) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == socket) {
            return i;
        }
    }
    return -1;
}

int find_client_by_uid(int uid) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != -1 && clients[i].uid == uid) {
            return i;
        }
    }
    return -1;
}

// Conta o número de clientes de um tipo específico
int count_clients_by_type(int type) {
    int count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != -1 && clients[i].type == type) {
            count++;
        }
    }
    return count;
}

// Remove um cliente da lista
void remove_client(int index) {
    if (index >= 0 && index < MAX_CLIENTS) {
        if (clients[index].socket != -1) {
            closesocket(clients[index].socket);
            printf("Cliente %d desconectado.\n", clients[index].uid);
            clients[index].socket = -1;
            clients[index].uid = 0;
            clients[index].type = CLIENT_NONE;
        }
    }
}

// Envia mensagem periódica do servidor
void send_periodic_message() {
    msg_t msg;
    time_t now = time(NULL);
    int display_clients = count_clients_by_type(CLIENT_DISPLAY);
    int uptime = (int)difftime(now, server_start_time);
    int hours = uptime / 3600;
    int minutes = (uptime % 3600) / 60;
    int seconds = uptime % 60;
    
    // Limpar a estrutura
    memset(&msg, 0, sizeof(msg));
    
    // Preparar a mensagem
    msg.type = TYPE_MSG;
    msg.orig_uid = 0;  // Mensagem do servidor
    msg.dest_uid = 0;  // Para todos
    
    snprintf((char*)msg.text, MAX_TEXT, 
             "%s | Clientes conectados: %d | Tempo online: %02d:%02d:%02d",
             SERVER_NAME, display_clients, hours, minutes, seconds);
    
    msg.text_len = strlen((char*)msg.text) + 1;  // +1 para o terminador nulo
    
    // Enviar para todos os clientes de exibição
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != -1 && clients[i].type == CLIENT_DISPLAY) {
            if (write_msg(clients[i].socket, &msg) <= 0) {
                printf("Erro ao enviar mensagem periódica para cliente %d\n", clients[i].uid);
                remove_client(i);
            }
        }
    }
}

// No Windows não temos o sigalrm_handler. A função timerThreadProc faz o trabalho.

// Processa uma mensagem OI de um cliente
void process_hello_message(int client_index, msg_t *msg) {
    int uid = msg->orig_uid;
    msg_t response;
    
    // Verificar se é um cliente válido
    if ((uid >= 1 && uid <= 999) || (uid >= 1001 && uid <= 1999)) {
        // Verificar se o ID já está em uso
        if (find_client_by_uid(uid) != -1 && find_client_by_uid(uid) != client_index) {
            printf("ID %d já está em uso. Conexão rejeitada.\n", uid);
            remove_client(client_index);
            return;
        }
        
        // Se for um cliente de exibição, verificar se o ID+1000 já está em uso
        if (uid >= 1 && uid <= 999) {
            if (find_client_by_uid(uid + 1000) != -1) {
                printf("ID %d+1000 já está em uso. Conexão rejeitada.\n", uid);
                remove_client(client_index);
                return;
            }
            clients[client_index].type = CLIENT_DISPLAY;
        } 
        // Se for um cliente de envio, verificar se o ID-1000 já está em uso
        else if (uid >= 1001 && uid <= 1999) {
            clients[client_index].type = CLIENT_SENDER;
        }
        
        // Atualizar o ID do cliente
        clients[client_index].uid = uid;
        
        // Responder com uma mensagem OI
        memset(&response, 0, sizeof(response));
        response.type = TYPE_OI;
        response.orig_uid = uid;
        response.dest_uid = 0;
        response.text_len = 0;
        
        if (write_msg(clients[client_index].socket, &response) <= 0) {
            printf("Erro ao enviar resposta OI para cliente %d\n", uid);
            remove_client(client_index);
            return;
        }
        
        printf("Cliente %d registrado com sucesso (tipo: %s).\n", 
               uid, (clients[client_index].type == CLIENT_DISPLAY) ? "exibição" : "envio");
    } else {
        printf("ID %d inválido. Conexão rejeitada.\n", uid);
        remove_client(client_index);
    }
}

// Processa uma mensagem TCHAU de um cliente
void process_goodbye_message(int client_index, msg_t *msg) {
    int uid = clients[client_index].uid;
    
    printf("Cliente %d enviou mensagem TCHAU.\n", uid);
    
    // Se for um cliente de envio, verificar se há um cliente de exibição correspondente
    if (clients[client_index].type == CLIENT_SENDER && uid >= 1001 && uid <= 1999) {
        int display_index = find_client_by_uid(uid - 1000);
        if (display_index != -1) {
            printf("Fechando conexão com cliente de exibição %d associado.\n", uid - 1000);
            remove_client(display_index);
        }
    }
    
    remove_client(client_index);
}

// Processa uma mensagem MSG de um cliente
void process_message(int client_index, msg_t *msg) {
    int uid = clients[client_index].uid;
    
    // Verificar se o ID de origem na mensagem corresponde ao ID registrado do cliente
    if (msg->orig_uid != uid) {
        printf("ID de origem na mensagem (%d) não corresponde ao ID do cliente (%d). Mensagem descartada.\n",
               msg->orig_uid, uid);
        return;
    }
    
    printf("Mensagem recebida de %d para %d: %s\n", uid, msg->dest_uid, msg->text);
    
    // Se o destino for 0, enviar para todos os clientes de exibição
    if (msg->dest_uid == 0) {
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket != -1 && clients[i].type == CLIENT_DISPLAY) {
                if (write_msg(clients[i].socket, msg) <= 0) {
                    printf("Erro ao enviar mensagem para cliente %d\n", clients[i].uid);
                    remove_client(i);
                }
            }
        }
    } 
    // Enviar apenas para o cliente de exibição específico
    else {
        int dest_index = find_client_by_uid(msg->dest_uid);
        if (dest_index != -1 && clients[dest_index].type == CLIENT_DISPLAY) {
            if (write_msg(clients[dest_index].socket, msg) <= 0) {
                printf("Erro ao enviar mensagem para cliente %d\n", msg->dest_uid);
                remove_client(dest_index);
            }
        } else {
            printf("Cliente de destino %d não encontrado ou não é um cliente de exibição.\n", msg->dest_uid);
        }
    }
}

// Lida com a mensagem recebida de um cliente
void process_client_message(int client_socket) {
    int client_index = find_client_by_socket(client_socket);
    if (client_index == -1) {
        printf("Socket %d não encontrado na lista de clientes.\n", client_socket);
        closesocket(client_socket);
        return;
    }
    
    msg_t msg;
    memset(&msg, 0, sizeof(msg));
    
    // Receber a mensagem
    if (read_msg(client_socket, &msg) <= 0) {
        printf("Erro ao receber mensagem do cliente %d\n", clients[client_index].uid);
        remove_client(client_index);
        return;
    }
    
    // Processar a mensagem de acordo com o tipo
    switch (msg.type) {
        case TYPE_OI:
            process_hello_message(client_index, &msg);
            break;
        case TYPE_TCHAU:
            process_goodbye_message(client_index, &msg);
            break;
        case TYPE_MSG:
            process_message(client_index, &msg);
            break;
        default:
            printf("Tipo de mensagem desconhecido: %d\n", msg.type);
            break;
    }
}

// Implementações das funções definidas em common.h
int send_full(int fd, const void *buf, size_t len) {
    size_t sent = 0;
    ssize_t ret;
    
    while (sent < len) {
        ret = send(fd, (char*)buf + sent, len - sent, 0);
        if (ret <= 0) {
            if (WSAGetLastError() == EINTR) continue;  // Interrompido por sinal, tentar novamente
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
            if (WSAGetLastError() == EINTR) continue;  // Interrompido por sinal, tentar novamente
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

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    fd_set read_fds, master_fds;
    int max_fd, new_socket;
    int port = PORT_DEFAULT;
    
    // Inicializar Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        die("Falha ao inicializar Winsock");
    }
    
    // Se porta for especificada como argumento
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    // Inicializar o array de clientes
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = -1;
        clients[i].uid = 0;
        clients[i].type = CLIENT_NONE;
    }
    
    // Registrar horário de inicio
    server_start_time = time(NULL);
    
    // No Windows, não podemos usar signal(SIGALRM) e alarm() como no Unix
    // Vamos configurar um temporizador usando Windows API    HANDLE timerThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)timerThreadProc, NULL, 0, NULL);
    if (timerThread == NULL) {
        die("Erro ao criar o thread do temporizador");
    }
    
    // Criar o socket do servidor
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        die("Erro ao criar o socket do servidor");
    }
    
    // Configurar o endereço do servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    // Permitir reutilização do endereço
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        die("Erro ao configurar opção de socket");
    }
    
    // Bind do socket do servidor
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        die("Erro ao bind do socket do servidor");
    }
    
    // Listen do socket do servidor
    if (listen(server_socket, 5) < 0) {
        die("Erro ao listen do socket do servidor");
    }
    
    printf("Servidor %s iniciado na porta %d. Aguardando conexões...\n", SERVER_NAME, port);
    
    // Inicializar os conjuntos de descritores
    FD_ZERO(&master_fds);
    FD_SET(server_socket, &master_fds);
    max_fd = server_socket;
    
    while (1) {
        // Copiar o conjunto mestre para o conjunto de leitura
        read_fds = master_fds;
        
        // Esperar por atividade em qualquer socket
        struct timeval timeout;
        timeout.tv_sec = 1;  // Timeout de 1 segundo para permitir verificação periódica
        timeout.tv_usec = 0;
        
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
          if (activity < 0) {
            if (WSAGetLastError() == EINTR) {
                // Interrompido por um sinal - verificar se o temporizador expirou
                if (timer_expired) {
                    send_periodic_message();
                    timer_expired = 0;
                }
                continue;
            } else {
                die("Erro em select");
            }
        } else if (activity == 0) {
            // Timeout - verificar se o temporizador expirou
            if (timer_expired) {
                send_periodic_message();
                timer_expired = 0;
            }
            continue;
        }
        
        // Verificar se o socket do servidor tem uma nova conexão
        if (FD_ISSET(server_socket, &read_fds)) {
            new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
            if (new_socket < 0) {
                perror("Erro ao aceitar conexão de cliente");
                continue;
            }
            
            printf("Nova conexão de %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            
            // Adicionar novo cliente à lista
            int added = 0;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket == -1) {
                    clients[i].socket = new_socket;
                    clients[i].connect_time = time(NULL);
                    clients[i].uid = 0;  // Será definido quando a mensagem OI for recebida
                    clients[i].type = CLIENT_NONE;
                    added = 1;
                    break;
                }
            }
            
            if (!added) {
                printf("Número máximo de clientes atingido. Conexão rejeitada.\n");
                closesocket(new_socket);
            } else {
                // Adicionar o novo socket ao conjunto mestre
                FD_SET(new_socket, &master_fds);
                if (new_socket > max_fd) {
                    max_fd = new_socket;
                }
            }
        }
        
        // Verificar os clientes existentes para atividade
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clients[i].socket;
            
            if (sd > 0 && FD_ISSET(sd, &read_fds)) {
                process_client_message(sd);
                
                // Se o cliente foi removido, também remover do conjunto mestre
                if (clients[i].socket == -1) {
                    FD_CLR(sd, &master_fds);
                }
            }
        }
        
        // Verificar se o temporizador expirou
        if (timer_expired) {
            send_periodic_message();
            timer_expired = 0;
        }
    }
    
    return 0;
}