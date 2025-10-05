#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

typedef enum {
    PACKET_TYPE_DESC = 0, // Pacote de descrição.
    PACKET_TYPE_REQ = 1, // Pacote de requisição.
    PACKET_TYPE_DESC_ACK = 2, // Confirmação de recebimento do pacote de descrição.
    PACKET_TYPE_REQ_ACK = 3 // Confirmação de recebimento do pacote de requisição.
} packet_type_t;

struct requisicao {
    uint32_t dest_addr; // Endereço IP do cliente destino
    uint32_t value; // Valor da transferência
};

struct requisicao_ack {
    uint32_t seqn; // Número de sequência que está sendo feito o ack
    uint32_t new_balance; // Novo saldo do cliente origem
};

typedef struct packet {
    uint16_t type; // Tipo do pacote (DESC — REQ — DESC_ACK — REQ_ACK )
    uint32_t seqn; // Número de sequência de uma requisição
    union {
        struct requisicao req;
        struct requisicao_ack ack;
    };
} packet;

#endif // PROTOCOL_H
