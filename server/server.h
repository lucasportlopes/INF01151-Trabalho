#ifndef SERVER_H
#define SERVER_H


#include "interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
// #include <sys/types.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <netdb.h>

#define BUF_SIZE 256

enum packet_type {
  DESC,
  REQ,
  DESC_ACK,
  REQ_ACK
};

struct requisicao {
    uint32_t dest_addr; // Endereço IP do cliente destino
    uint32_t value; // Valor da transferência
};

struct requisicao_ack {
    uint32_t seqn; // Número de sequência que está sendo feito o ack
    uint32_t new_balance; // Novo saldo do cliente origem
};

typedef struct {
    enum packet_type type; // Tipo do pacote (DESC | REQ | DESC_ACK | REQ_ACK )
    uint32_t seqn; // Número de sequência de uma requisição
    union {
        struct requisicao req;
        struct requisicao_ack ack;
    };
} packet;

#endif // SERVER_H
