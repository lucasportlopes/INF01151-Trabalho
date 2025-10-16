#include "processing.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Tabela temporária de clientes para simulação
#define MAX_CLIENTS 10
static client_t client_table[MAX_CLIENTS];
static int num_clients = 0;
extern pthread_mutex_t data_mutex;

void* process_request_thread(void* args) {
    if (args == NULL) {
        pthread_exit(NULL);
    }
    thread_args_t* thread_args = (thread_args_t*)args;
    handle_request(thread_args->sockfd, &thread_args->client_addr, thread_args->addr_len, &thread_args->req_packet);
    free(thread_args);
    pthread_exit(NULL);
}

client_t* find_or_add_client(uint32_t ip_addr) {
    for (int i = 0; i < num_clients; i++) {
        if (client_table[i].ip_addr == ip_addr) {
            return &client_table[i];
        }
    }
    if (num_clients < MAX_CLIENTS) {
        client_t* new_client = &client_table[num_clients];
        new_client->ip_addr = ip_addr;
        new_client->last_req_id = 0;
        new_client->balance = 100;
        new_client->in_use = 1;
        total_balance += 100;
        
        num_clients++;
        return new_client;
    }
    return NULL; // Tabela cheia
}

client_t* find_client_by_ip(uint32_t ip_addr) {
    for (int i = 0; i < num_clients; i++) {
        if (client_table[i].ip_addr == ip_addr) {
            return &client_table[i];
        }
    }
    return NULL;
}

void handle_request(int sockfd, const struct sockaddr_in *client_addr, socklen_t addr_len, const packet *req_packet) {
    pthread_mutex_lock(&data_mutex);
    if (client_addr == NULL || req_packet == NULL) {
        pthread_mutex_unlock(&data_mutex);
        return;
    }

    if (req_packet->type != REQ) {
        fprintf(stderr, "[srv] handle_request: tipo invalido %u\n", (unsigned)req_packet->type);
        pthread_mutex_unlock(&data_mutex);
        return;
    }

    char client_ip_str[INET_ADDRSTRLEN];
    char dest_ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip_str, INET_ADDRSTRLEN);
    
    struct in_addr dest_addr_struct;
    dest_addr_struct.s_addr = req_packet->req.dest_addr;
    inet_ntop(AF_INET, &dest_addr_struct, dest_ip_str, INET_ADDRSTRLEN);

    // Encontra/adiciona o cliente de origem e o de destino
    client_t* source_client = find_or_add_client(client_addr->sin_addr.s_addr);
    client_t* dest_client = find_client_by_ip(req_packet->req.dest_addr);

    if(source_client == NULL) {
        fprintf(stderr, "[srv] handle_request: tabela de clientes cheia\n");
        pthread_mutex_unlock(&data_mutex);
        return;
    }

    if (req_packet->seqn <= source_client->last_req_id) {
        log_duplicate_request(client_ip_str, dest_ip_str, req_packet->seqn, req_packet->req.value, num_transactions, total_transferred, total_balance);
    } else {
        if (dest_client == NULL) {
            fprintf(stderr, "Cliente de destino %s não encontrado.\n", dest_ip_str);

        } else if (req_packet->req.value == 0) {
            source_client->last_req_id = req_packet->seqn;

        } else if (source_client->balance < req_packet->req.value) {
            fprintf(stderr, "Saldo insuficiente para o cliente %s.\n", client_ip_str);

        } else {
            source_client->balance -= req_packet->req.value;
            dest_client->balance += req_packet->req.value;
            
            num_transactions++;
            total_transferred += req_packet->req.value;
            
            source_client->last_req_id = req_packet->seqn;
            
            log_request(client_ip_str, dest_ip_str, req_packet->seqn, req_packet->req.value, num_transactions, total_transferred, total_balance);
        }
    }

    pthread_mutex_unlock(&data_mutex);

    packet ack_packet;
    memset(&ack_packet, 0, sizeof(ack_packet));
    ack_packet.type = REQ_ACK;
    ack_packet.seqn = req_packet->seqn;
    ack_packet.ack.seqn = source_client->last_req_id;
    ack_packet.ack.new_balance = source_client->balance;

    if (sendto(sockfd, &ack_packet, sizeof(ack_packet), 0, (const struct sockaddr *)client_addr, addr_len) < 0) {
        perror("sendto ack");
    }
    printf("\n");
}
