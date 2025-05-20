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
    char buffer[MAX_TEXT];
    int dest_uid;
    
    // Verificar argumentos da linha de comando
    if (argc < 2) {
        printf("Uso: %s <ID> [IP] [PORTA]\n", argv[0]);
        printf("Exemplo: %s 1001 192.168.1.100 9000\n", argv[0]);
        return 1;
    }
    
    // Obter ID do cliente
    uid = atoi(argv[1]);
    if (uid < 1001 || uid > 1999) {
        printf("ID inválido. Deve ser um número entre 1001 e 1999.\n");
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
    
    printf("Registro concluído com sucesso.\n");
    printf("Instruções:\n");
    printf("- Para enviar uma mensagem para todos, digite: 0 <mensagem>\n");
    printf("- Para enviar uma mensagem para um cliente específico, digite: <ID> <mensagem>\n");
    printf("- Para sair, digite: exit\n\n");
    
    // Loop principal para enviar mensagens
    while (1) {
        printf("> ");
        fflush(stdout);
        
        // Ler entrada do usuário
        if (fgets(buffer, MAX_TEXT, stdin) == NULL) {
            break;
        }
        
        // Verificar se é comando para sair
        if (strncmp(buffer, "exit", 4) == 0) {
            printf("Enviando mensagem de despedida para o servidor...\n");
            
            // Enviar mensagem TCHAU para o servidor
            memset(&msg, 0, sizeof(msg));
            msg.type = TYPE_TCHAU;
            msg.orig_uid = uid;
            msg.dest_uid = 0;
            msg.text_len = 0;
            
            write_msg(client_socket, &msg);
            break;
        }
        
        // Processar a entrada: formato "destino mensagem"
        char *message = strchr(buffer, ' ');
        if (message == NULL) {
            printf("Formato inválido. Use: <ID> <mensagem>\n");
            continue;
        }
        
        // Separar o ID de destino da mensagem
        *message = '\0';  // Substituir o espaço por \0 para terminar a string do destino
        message++;        // Avançar para o início da mensagem
        
        // Converter o destino para inteiro
        dest_uid = atoi(buffer);
        
        // Remover o \n do final da mensagem
        size_t len = strlen(message);
        if (len > 0 && message[len-1] == '\n') {
            message[len-1] = '\0';
            len--;
        }
        
        // Verificar se a mensagem está vazia
        if (len == 0) {
            printf("Mensagem vazia. Tente novamente.\n");
            continue;
        }
        
        // Verificar se a mensagem é muito longa
        if (len > MAX_TEXT - 1) {
            printf("Mensagem muito longa. Máximo: %d caracteres.\n", MAX_TEXT - 1);
            continue;
        }
        
        // Enviar mensagem para o servidor
        memset(&msg, 0, sizeof(msg));
        msg.type = TYPE_MSG;
        msg.orig_uid = uid;
        msg.dest_uid = dest_uid;
        strncpy((char*)msg.text, message, MAX_TEXT - 1);
        msg.text_len = strlen((char*)msg.text) + 1;  // +1 para o terminador nulo
        
        if (write_msg(client_socket, &msg) <= 0) {
            printf("Erro ao enviar mensagem para o servidor.\n");
            break;
        }
        
        printf("Mensagem enviada com sucesso.\n");
    }
    
    // Fechar conexão
    closesocket(client_socket);
    WSACleanup();
    
    return 0;
}