// protocol.h
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// Puerto del servidor [cite: 26]
#define SERVER_PORT 20252
// Tamaño máximo de payload recomendado [cite: 32]
#define MAX_PAYLOAD_SIZE 1478
#define BUF_SIZE 1500

// Tipos de mensaje [cite: 29]
#define TYPE_HELLO 1
#define TYPE_WRQ   2
#define TYPE_DATA  3
#define TYPE_ACK   4
#define TYPE_FIN   5

// Estructura de la PDU (sin empaquetado estricto por simplicidad, 
// pero en producción usar __attribute__((packed)))
struct pdu {
    uint8_t type;
    uint8_t seq_num;
    char payload[MAX_PAYLOAD_SIZE];
};

#endif