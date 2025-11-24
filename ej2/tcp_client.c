// tcp_client.c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define SERVER_PORT      20252
#define MIN_PAYLOAD_SIZE 500
#define MAX_PAYLOAD_SIZE 1000

static uint64_t now_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

// Envía todo el buffer por TCP (maneja partial sends)
static int send_all(int sockfd, const void *buf, size_t len) {
    const char *p = buf;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(sockfd, p + sent, len - sent, 0);
        if (n <= 0) {
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr,
                "Uso: %s <IP Servidor> -d <delay_ms> -N <duracion_s>\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    const char *server_ip = argv[1];
    int delay_ms = -1;
    int duration_s = -1;

    // parseo simple de -d y -N
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            delay_ms = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-N") == 0 && i + 1 < argc) {
            duration_s = atoi(argv[++i]);
        }
    }

    if (delay_ms <= 0 || duration_s <= 0) {
        fprintf(stderr,
                "Parámetros inválidos. Ejemplo: %s 192.168.20.144 -d 50 -N 10\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    int sockfd;
    struct sockaddr_in serv_addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(SERVER_PORT);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sockfd);
        return EXIT_FAILURE;
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sockfd);
        return EXIT_FAILURE;
    }

    printf("Conectado a %s:%d. delay=%d ms, duracion=%d s\n",
           server_ip, SERVER_PORT, delay_ms, duration_s);

    uint64_t start_us = now_us();
    uint64_t duration_us = (uint64_t)duration_s * 1000000ULL;

    // buffer suficientemente grande para la PDU máxima
    char pdu[8 + MAX_PAYLOAD_SIZE + 1];

    // inicializar random
    srand((unsigned int)start_us);

    while (1) {
        uint64_t t_now = now_us();
        if (t_now - start_us >= duration_us) {
            break; // terminó la prueba
        }

        uint64_t origin_ts_us = t_now;

        // elegir tamaño de payload entre 500 y 1000
        int payload_len = MIN_PAYLOAD_SIZE +
            rand() % (MAX_PAYLOAD_SIZE - MIN_PAYLOAD_SIZE + 1);

        // armar PDU: 8 bytes timestamp + payload + '|'
        memcpy(pdu, &origin_ts_us, sizeof(uint64_t));
        memset(pdu + 8, 0x20, payload_len);  // payload = espacios
        pdu[8 + payload_len] = '|';

        size_t pdu_len = 8 + (size_t)payload_len + 1;

        if (send_all(sockfd, pdu, pdu_len) < 0) {
            perror("send_all");
            break;
        }

        // esperar d ms
        usleep((unsigned int)delay_ms * 1000U);

    }

    close(sockfd);
    return EXIT_SUCCESS;
}
