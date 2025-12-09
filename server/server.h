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

#define MAX_SERVERS 4
#define MAX_CLIENTS 10

typedef enum {
    REPLICA_PRIMARIO,   
    REPLICA_SECUNDARIO   
} server_role_t;

typedef struct {
    int id;    
    struct sockaddr_in addr;
} replica_t;

typedef struct {
    uint32_t ip_addr;
    uint32_t last_req_id;
    uint32_t balance;
    int in_use; 
} client_t;

extern int num_transactions;
extern int total_transferred;
extern int num_clients;
extern int total_balance;
extern int my_id;                  
extern int current_leader_id;   
extern int num_servers;        
extern replica_t server_list[MAX_SERVERS]; 
extern client_t client_table[MAX_CLIENTS];
extern server_role_t current_role; 

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
  PING,
  SERVER_JOIN
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
    int num_clients; // Número de clientes na tabela
    int num_transactions; // Número total de transações processadas
    int total_transferred; // Valor total transferido
    int total_balance; // Saldo total entre todos os clientes
    client_t client_table[MAX_CLIENTS]; // Tabela de clientes completa
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
        struct replication_msg rep;
        struct leader_info leader;
        struct send_info send_new;
    };
} packet;

typedef struct {
    int sockfd;
    struct sockaddr_in client_addr;
    socklen_t addr_len;
    packet req_packet;
} thread_args_t;
#endif // SERVER_H
