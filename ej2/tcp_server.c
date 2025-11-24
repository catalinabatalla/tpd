// tcp_server.c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define SERVER_PORT 20252
#define BUF_SIZE    4096

// Convierte gettimeofday() a microsegundos desde epoch
static uint64_t now_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

int main(void) {
    int listenfd, connfd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t cli_len = sizeof(cli_addr);

    // 1) Crear socket TCP
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port        = htons(SERVER_PORT);

    if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    if (listen(listenfd, 1) < 0) {
        perror("listen");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    printf("Servidor TCP escuchando en puerto %d...\n", SERVER_PORT);

    connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &cli_len);
    if (connfd < 0) {
        perror("accept");
        close(listenfd);
        exit(EXIT_FAILURE);
    }
    printf("Cliente conectado.\n");

    FILE *csv = fopen("owd_results.csv", "w");
    if (!csv) {
        perror("fopen csv");
        close(connfd);
        close(listenfd);
        exit(EXIT_FAILURE);
    }
    // podés dejar sin header si querés
    fprintf(csv, "n,delay_s\n");

    char buf[BUF_SIZE];
    int used = 0;          // bytes válidos en buf
    int measurement = 0;   // contador de mediciones

    while (1) {
        ssize_t n = read(connfd, buf + used, BUF_SIZE - used);
        if (n < 0) {
            perror("read");
            break;
        }
        if (n == 0) {
            // FIN de conexión
            printf("Cliente cerró la conexión.\n");
            break;
        }

        used += (int)n;

        // Procesar tantas PDUs completas como haya en el buffer
        int processed = 0;
        while (used - processed >= 9) {
            // Buscamos delimitador '|' (0x7C) a partir del byte 8
            int start = processed;
            int min_index = start + 8; // timestamp ocupa 8 bytes
            int found = -1;
            for (int i = min_index; i < used; i++) {
                if ((unsigned char)buf[i] == '|') {
                    found = i;
                    break;
                }
            }
            if (found == -1) {
                // No hay delimitador completo todavía
                break;
            }

            int pdu_len = found - start + 1; // desde start hasta incluido '|'
            if (pdu_len < 509 || pdu_len > 1009) {
                // PDU de tamaño inválido, la descartamos (en TP real podrías loguear)
                fprintf(stderr, "PDU invalida (len=%d), descartando\n", pdu_len);
                processed = found + 1;
                continue;
            }

            // Tenemos una PDU completa en buf[start .. start+pdu_len-1]
            uint64_t origin_ts_us = 0;
            memcpy(&origin_ts_us, buf + start, sizeof(uint64_t));

            uint64_t dest_ts_us = now_us();
            double delay_s = (double)(dest_ts_us - origin_ts_us) / 1e6;

            measurement++;
            fprintf(csv, "%d,%.6f\n", measurement, delay_s);

            processed = start + pdu_len;
        }

        // Compactar buffer dejando sólo bytes no procesados
        if (processed > 0) {
            memmove(buf, buf + processed, used - processed);
            used -= processed;
        }

        // Si el buffer se llena demasiado y no se encontró '|' algo raro pasa
        if (used == BUF_SIZE) {
            fprintf(stderr, "Buffer lleno sin encontrar delimitador; reseteando.\n");
            used = 0;
        }
    }

    fclose(csv);
    close(connfd);
    close(listenfd);
    return 0;
}
