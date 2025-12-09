#ifndef SERVER_H
#define SERVER_H

#include "interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <errno.h>
#include <pthread.h>

#define MAX_SERVERS = 4;

extern int num_transactions;
extern int total_transferred;
extern int total_balance;
extern server_role_t current_role; // Variável global para saber se sou Líder ou Backup
extern int my_id;                  // Variável global com meu ID
extern int current_leader_id;   // ID do Líder atual
extern int num_servers;         // Quantidade de servidores na lista
extern replica_t server_list[MAX_SERVERS]; // Lista de todos os servidores (Líder + Backups)

typedef enum {
    REPLICA_PRIMARIO,   
    REPLICA_SECUNDARIO   
} server_role_t;

typedef struct {
    int id;               // ID do servidor (1, 2, 3...) - Importante para eleição depois
    struct sockaddr_in addr; // Estrutura pronta para sendto()
} replica_t;

enum packet_type {
  DESC,
  REQ,
  DESC_ACK,
  REQ_ACK,
  REP_UPDATE,  
  REP_CONFIRM,
  ELECTION,    
  COORDINATOR,
  FIND_LEADER,
  PING
};

struct requisicao {
    uint32_t dest_addr; // Endereço IP do cliente destino
    uint32_t value; // Valor da transferência
};

struct requisicao_ack {
    uint32_t seqn; // Número de sequência que está sendo feito o ack
    uint32_t new_balance; // Novo saldo do cliente origem
};

struct replication_msg
{
    uint32_t client_ip;      // ID/IP do cliente afetado
    uint32_t new_balance;    // Novo saldo para atualizar no backup
    uint32_t transaction_id; // ID da transação (para consistência)
};

struct leader_info {
    int leader_id;                // ID do líder
    int nr_servers;               // Número de servidores na rede
    replica_t servers[MAX_SERVERS]; // Lista de servidores
};

struct send_info {
    replica_t new_server;
};

typedef struct {
    enum packet_type type; // Tipo do pacote (DESC | REQ | DESC_ACK | REQ_ACK )
    uint32_t seqn; // Número de sequência de uma requisição
    union {
        struct requisicao req;
        struct requisicao_ack ack;
    };
} packet;

typedef struct {
    enum packet_type type; // Tipo do pacote (REP_UPDATE | REP_CONFIRM | ELECTION | COORDINATOR | FIND_LEADER )
    uint32_t seqn; // Número de sequência de uma requisição
    union {
        struct replication_msg rep;
        struct leader_info leader;
        struct send_info send_new;
    }
} packet_servers;

typedef struct {
    int sockfd;
    struct sockaddr_in client_addr;
    socklen_t addr_len;
    packet req_packet;
} thread_args_t;
#endif // SERVER_H
