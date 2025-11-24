// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include "protocol.h"

#define MAX_CLIENTS 10

// Estados del cliente
typedef enum { STATE_NONE, STATE_AUTH, STATE_WRQ_DONE, STATE_DATA } client_state_t;

typedef struct {
    struct sockaddr_in addr;
    int active;
    client_state_t state;
    FILE *fp;
    uint8_t expected_seq;
} client_t;

client_t clients[MAX_CLIENTS];

void init_clients() {
    for (int i = 0; i < MAX_CLIENTS; i++) clients[i].active = 0;
}

// Busca cliente por IP/Puerto o devuelve un slot libre
int get_client_index(struct sockaddr_in *cli_addr) {
    int free_idx = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            if (clients[i].addr.sin_addr.s_addr == cli_addr->sin_addr.s_addr &&
                clients[i].addr.sin_port == cli_addr->sin_port) {
                return i;
            }
        } else {
            if (free_idx == -1) free_idx = i;
        }
    }
    return free_idx; // Retorna índice libre si es nuevo
}

void send_ack(int sockfd, struct sockaddr_in *addr, uint8_t seq, char *msg) {
    struct pdu response;
    response.type = TYPE_ACK;
    response.seq_num = seq;
    memset(response.payload, 0, MAX_PAYLOAD_SIZE);
    if(msg) strncpy(response.payload, msg, MAX_PAYLOAD_SIZE);
    
    // PDU total size: 2 bytes header + payload length
    sendto(sockfd, &response, 2 + (msg ? strlen(msg) : 0), 0, 
           (struct sockaddr *)addr, sizeof(*addr));
}

int main() {
    int sockfd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t len = sizeof(cli_addr);
    char buffer[BUF_SIZE];

    init_clients();

    // Crear socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(SERVER_PORT);

    if (bind(sockfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    printf("Servidor UDP escuchando en puerto %d...\n", SERVER_PORT);

    fd_set readfds;
    
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        // select() bloqueante esperando datos
        if (select(sockfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("Select error");
            continue;
        }

        if (FD_ISSET(sockfd, &readfds)) {
            int n = recvfrom(sockfd, buffer, BUF_SIZE, 0, (struct sockaddr *)&cli_addr, &len);
            if (n < 2) continue; // Paquete invalido (muy corto)

            struct pdu *packet = (struct pdu *)buffer;
            int idx = get_client_index(&cli_addr);

            if (idx == -1) {
                printf("Servidor lleno, ignorando cliente.\n");
                continue;
            }

            client_t *cli = &clients[idx];
            
            // Si es un cliente nuevo en este slot
            if (!cli->active) {
                cli->active = 1;
                cli->addr = cli_addr;
                cli->state = STATE_NONE;
                cli->expected_seq = 0;
            }

            // --- MÁQUINA DE ESTADOS ---
            
            // FASE 1: HELLO [cite: 36]
            if (packet->type == TYPE_HELLO && cli->state == STATE_NONE) {
                printf("Cliente %d: HELLO recibido con credencial: %.*s\n", idx, n-2, packet->payload);
                // Aquí validarías credenciales. Asumimos OK.
                char credencial_valida[] = "g21-0e29";

                if (strncmp(packet->payload, credencial_valida, strlen(credencial_valida)) == 0) {
                    // Credencial OK -> Enviar ACK vacío (éxito)
                    send_ack(sockfd, &cli_addr, 0, NULL);
                    cli->state = STATE_AUTH;
                    cli->expected_seq = 1;
                } else {
                    // Credencial MALA -> Enviar ACK con mensaje de error
                    printf("Cliente %d: Credencial invalida rechazadas.\n", idx);
                    send_ack(sockfd, &cli_addr, 0, "Credencial Invalida");
                    // Mantenemos el estado en NONE o reiniciamos
                    cli->active = 0; 
                }

                // send_ack(sockfd, &cli_addr, 0, NULL);
                // cli->state = STATE_AUTH;
                // cli->expected_seq = 1; // Próximo seq esperado
            }
            // FASE 2: WRQ 
            else if (packet->type == TYPE_WRQ && cli->state == STATE_AUTH) {
                if (packet->seq_num != 1) continue; // Seq incorrecto

                char filename[20];
                memset(filename, 0, 20);
                strncpy(filename, packet->payload, n - 2);
                
                printf("Cliente %d: WRQ para archivo %s\n", idx, filename);
                
                // Validar nombre (4-10 chars)
                if (strlen(filename) < 4 || strlen(filename) > 10) {
                   send_ack(sockfd, &cli_addr, 1, "Error Name");
                   // Resetear cliente o manejar error
                   continue;
                }

                char path[50];
                // sprintf(path, "uploads_%d_%s", idx, filename);
                strncpy(path, filename, 49);
                cli->fp = fopen(path, "wb");
                
                if (cli->fp) {
                    send_ack(sockfd, &cli_addr, 1, NULL);
                    cli->state = STATE_DATA;
                    cli->expected_seq = 0;
                } else {
                    send_ack(sockfd, &cli_addr, 1, "Error FS");
                }
            }
            // FASE 3: DATA [cite: 40]
            else if (packet->type == TYPE_DATA && cli->state == STATE_DATA) {
                if (packet->seq_num == cli->expected_seq) {
                    // Escribir en archivo (n - 2 bytes de header)
                    fwrite(packet->payload, 1, n - 2, cli->fp);
                    // Enviar ACK
                    send_ack(sockfd, &cli_addr, cli->expected_seq, NULL);
                    // Alternar secuencia (0->1, 1->0)
                    cli->expected_seq = 1 - cli->expected_seq;
                } else {
                    // Retransmisión de ACK anterior (paquete duplicado)
                    send_ack(sockfd, &cli_addr, 1 - cli->expected_seq, NULL);
                }
            }
            // FASE 4: FIN [cite: 42]
            else if (packet->type == TYPE_FIN && cli->state == STATE_DATA) {
                printf("Cliente %d: FIN recibido. Cerrando.\n", idx);
                if (cli->fp) fclose(cli->fp);
                send_ack(sockfd, &cli_addr, packet->seq_num, NULL);
                
                // Liberar slot
                cli->active = 0;
                cli->fp = NULL;
            }
            else {
                // Paquete fuera de secuencia o estado incorrecto: ignorar silenciosamente [cite: 34]
            }
        }
    }
    return 0;
}