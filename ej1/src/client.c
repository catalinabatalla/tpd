// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include "protocol.h"

// Función auxiliar para enviar y esperar ACK con reintentos
int send_and_wait(int sockfd, struct sockaddr_in *serv_addr, struct pdu *packet, int data_len) {
    char buffer[BUF_SIZE];
    struct pdu *ack;
    socklen_t len = sizeof(*serv_addr);
    int retries = 0;
    
    while (retries < 5) { // Max 5 reintentos
        // Enviar paquete
        sendto(sockfd, packet, 2 + data_len, 0, (struct sockaddr *)serv_addr, sizeof(*serv_addr));
        
        // Intentar recibir ACK
        int n = recvfrom(sockfd, buffer, BUF_SIZE, 0, (struct sockaddr *)serv_addr, &len);
        
        if (n > 0) {
            ack = (struct pdu *)buffer;
            if (ack->type == TYPE_ACK && ack->seq_num == packet->seq_num) {
                return 1; // Éxito
            }
            // Si recibimos error en payload
            if (ack->type == TYPE_ACK && n > 2) {
                printf("Error del servidor: %.*s\n", n-2, ack->payload);
                return 0;
            }
        } else {
            printf("Timeout... reintentando\n");
        }
        retries++;
    }
    return 0; // Falló después de reintentos
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Uso: %s <IP Servidor> <Credencial> <Archivo Local> <Nombre Remoto>\n", argv[0]);
        return -1;
    }

    int sockfd;
    struct sockaddr_in serv_addr;
    struct timeval tv;

    // Configurar Timeout de 2 segundos
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Setear timeout en el socket
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);

    struct pdu packet;
    
    // --- FASE 1: HELLO ---
    printf("Enviando HELLO...\n");
    packet.type = TYPE_HELLO;
    packet.seq_num = 0;
    strncpy(packet.payload, argv[2], MAX_PAYLOAD_SIZE); // Credencial
    if (!send_and_wait(sockfd, &serv_addr, &packet, strlen(argv[2]))) {
        printf("Fallo HELLO\n"); 
        close(sockfd);
        return -1;
    }

    // --- FASE 2: WRQ ---
    printf("Enviando WRQ...\n");
    packet.type = TYPE_WRQ;
    packet.seq_num = 1;
    strncpy(packet.payload, argv[4], MAX_PAYLOAD_SIZE);  // Nombre remoto
    
    if (!send_and_wait(sockfd, &serv_addr, &packet, strlen(argv[4]))) {
        printf("Fallo WRQ\n");
        close(sockfd);
        return -1;
    }


    // --- FASE 3: DATA ---
    FILE *fp = fopen(argv[3], "rb"); // Archivo local
    if (!fp) { 
        perror("No se puede abrir archivo"); 
        close (sockfd); 
        return -1; }

    int bytes_read;
    int current_seq = 0;
    
    while ((bytes_read = fread(packet.payload, 1, MAX_PAYLOAD_SIZE, fp)) > 0) {
        packet.type = TYPE_DATA;
        packet.seq_num = current_seq;
        
        printf("Enviando DATA seq %d (%d bytes)...\n", current_seq, bytes_read);
        
        if (!send_and_wait(sockfd, &serv_addr, &packet, bytes_read)) {
            printf("Fallo DATA transmission\n"); 
            fclose(fp); 
            close(sockfd);
            return -1;
        }
        
        current_seq = 1 - current_seq; // Toggle 0/1
    }
    fclose(fp);

    // --- FASE 4: FIN ---
    printf("Enviando FIN...\n");
    packet.type = TYPE_FIN;
    packet.seq_num = current_seq;
    send_and_wait(sockfd, &serv_addr, &packet, 0);

    printf("Transferencia completada.\n");
    close(sockfd);
    return 0;
}