#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "../include/common.h"

#define ssize_t int

// Linkar com a biblioteca Winsock
#pragma comment(lib, "Ws2_32.lib")

int main(int argc, char *argv[]) {
    int client_socket;
    struct sockaddr_in server_addr;
    msg_t msg;
    int port = PORT_DEFAULT;
    int uid;
    char *server_ip = "127.0.0.1";  // IP padrão (localhost)
    
    // Verificar argumentos da linha de comando
    if (argc < 2) {
        printf("Uso: %s <ID> [IP] [PORTA]\n", argv[0]);
        printf("Exemplo: %s 123 192.168.1.100 9000\n", argv[0]);
        return 1;
    }
    
    // Obter ID do cliente
    uid = atoi(argv[1]);
    if (uid <= 0 || uid > 999) {
        printf("ID inválido. Deve ser um número entre 1 e 999.\n");
        return 1;
    }
    
    // Se fornecido, usar IP do servidor especificado
    if (argc > 2) {
        server_ip = argv[2];
    }
    
    // Se fornecida, usar porta especificada
    if (argc > 3) {
        port = atoi(argv[3]);
    }
    
    // Inicializar Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Falha ao inicializar Winsock.\n");
        return 1;
    }
    
    // Criar socket do cliente
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET) {
        printf("Erro ao criar socket. Erro: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    
    // Configurar endereço do servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
    
    // Conectar ao servidor
    printf("Conectando ao servidor %s:%d...\n", server_ip, port);
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Falha ao conectar. Erro: %d\n", WSAGetLastError());
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }
    
    printf("Conectado ao servidor. Enviando mensagem OI com ID %d...\n", uid);
    
    // Enviar mensagem OI para o servidor
    memset(&msg, 0, sizeof(msg));
    msg.type = TYPE_OI;
    msg.orig_uid = uid;
    msg.dest_uid = 0;
    msg.text_len = 0;
    
    if (write_msg(client_socket, &msg) <= 0) {
        printf("Erro ao enviar mensagem OI para o servidor.\n");
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }
    
    // Aguardar resposta OI do servidor
    if (read_msg(client_socket, &msg) <= 0) {
        printf("Erro ao receber resposta do servidor.\n");
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }
    
    // Verificar se a resposta é uma mensagem OI
    if (msg.type != TYPE_OI) {
        printf("Resposta inesperada do servidor (tipo %d). Encerrando.\n", msg.type);
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }
    
    printf("Registro concluído com sucesso. Aguardando mensagens...\n");
    
    // Loop principal para receber mensagens
    while (1) {
        // Receber mensagem do servidor
        if (read_msg(client_socket, &msg) <= 0) {
            printf("Conexão com o servidor perdida.\n");
            break;
        }
        
        // Processar mensagem recebida
        if (msg.type == TYPE_MSG) {
            // Verificar se é mensagem privada ou para todos
            if (msg.dest_uid == 0) {
                if (msg.orig_uid == 0) {
                    // Mensagem do servidor
                    printf("[SERVIDOR]: %s\n", msg.text);
                } else {
                    // Mensagem para todos
                    printf("[%d para TODOS]: %s\n", msg.orig_uid, msg.text);
                }
            } else {
                // Mensagem privada
                printf("[%d para VOCÊ]: %s\n", msg.orig_uid, msg.text);
            }
        }
    }
    
    // Fechar conexão
    closesocket(client_socket);
    WSACleanup();
    
    return 0;
}