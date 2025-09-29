#ifndef CLIENT_H
#define CLIENT_H

#include "interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUF_SIZE 256

struct requisicao {
    uint32_t dest_addr; // Endereço IP do cliente destino
    uint32_t value; // Valor da transferência
};

struct requisicao_ack {
    uint32_t seqn; // Número de sequência que está sendo feito o ack
    uint32_t new_balance; // Novo saldo do cliente origem
};

typedef struct {
    uint16_t type; // Tipo do pacote (DESC | REQ | DESC_ACK | REQ_ACK )
    uint32_t seqn; // Número de sequência de uma requisição
    union {
        struct requisicao req;
        struct requisicao_ack ack;
    };
} packet;

#endif // CLIENT_H
