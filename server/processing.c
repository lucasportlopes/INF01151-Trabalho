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
static int readers = 0;
extern pthread_mutex_t write_mutex;
extern pthread_mutex_t read_mutex;
extern sem_t thread_limit;

void *process_request_thread(void *args)
{
    if (args == NULL)
    {
        pthread_exit(NULL);
    }
    sem_wait(&thread_limit);
    thread_args_t *thread_args = (thread_args_t *)args;
    handle_request(thread_args->sockfd, &thread_args->client_addr, thread_args->addr_len, &thread_args->req_packet);
    sem_post(&thread_limit);
    free(thread_args);
    pthread_exit(NULL);
}

client_t *find_or_add_client(uint32_t ip_addr)
{
    for (int i = 0; i < num_clients; i++)
    {
        if (client_table[i].ip_addr == ip_addr)
        {
            return &client_table[i];
        }
    }
    if (num_clients < MAX_CLIENTS)
    {
        client_t *new_client = &client_table[num_clients];
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

client_t *find_client_by_ip(uint32_t ip_addr)
{
    for (int i = 0; i < num_clients; i++)
    {
        if (client_table[i].ip_addr == ip_addr)
        {
            return &client_table[i];
        }
    }
    return NULL;
}

void handle_request(int sockfd, const struct sockaddr_in *client_addr, socklen_t addr_len, const packet *req_packet)
{
    pthread_mutex_lock(&read_mutex);
    readers++;
    if (readers == 1)
        pthread_mutex_lock(&write_mutex);
    pthread_mutex_unlock(&read_mutex);
    if (client_addr == NULL || req_packet == NULL)
    {
        pthread_mutex_lock(&read_mutex);
        readers--;
        if (readers == 0)
            pthread_mutex_unlock(&write_mutex);
        pthread_mutex_unlock(&read_mutex);
        return;
    }

    if (req_packet->type != REQ)
    {
        fprintf(stderr, "[srv] handle_request: tipo invalido %u\n", (unsigned)req_packet->type);
        pthread_mutex_lock(&read_mutex);
        readers--;
        if (readers == 0)
            pthread_mutex_unlock(&write_mutex);
        pthread_mutex_unlock(&read_mutex);
        return;
    }

    char client_ip_str[INET_ADDRSTRLEN];
    char dest_ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip_str, INET_ADDRSTRLEN);

    struct in_addr dest_addr_struct;
    dest_addr_struct.s_addr = req_packet->req.dest_addr;
    inet_ntop(AF_INET, &dest_addr_struct, dest_ip_str, INET_ADDRSTRLEN);

    // Encontra/adiciona o cliente de origem e o de destino
    client_t *source_client = find_or_add_client(client_addr->sin_addr.s_addr);
    client_t *dest_client = find_client_by_ip(req_packet->req.dest_addr);

    if (source_client == NULL)
    {
        fprintf(stderr, "[srv] handle_request: tabela de clientes cheia\n");
        pthread_mutex_lock(&read_mutex);
        readers--;
        if (readers == 0)
            pthread_mutex_unlock(&write_mutex);
        pthread_mutex_unlock(&read_mutex);
        return;
    }
    bool req_processed = false;
    bool value_was_zero = false;
    client_t copy_source_client = *source_client;
    client_t copy_dest_client = *dest_client;
    if (req_packet->seqn <= source_client->last_req_id)
    {
        pthread_mutex_lock(&read_mutex);
        readers--;
        if (readers == 0)
            pthread_mutex_unlock(&write_mutex);
        pthread_mutex_unlock(&read_mutex);
        pthread_thread_mutex_lock(&write_mutex);
        log_duplicate_request(client_ip_str, dest_ip_str, req_packet->seqn, req_packet->req.value, num_transactions, total_transferred, total_balance);
        pthread_thread_mutex_unlock(&write_mutex);
    }
    else
    {
        if (dest_client == NULL)
        {
            fprintf(stderr, "Cliente de destino %s não encontrado.\n", dest_ip_str);
        }
        else if (req_packet->req.value == 0)
        {
            copy_source_client->last_req_id = req_packet->seqn;
            req_processed = true;
            value_was_zero = true;
        }
        else if (copy_source_client->balance < req_packet->req.value)
        {
            fprintf(stderr, "Saldo insuficiente para o cliente %s.\n", client_ip_str);
        }
        else
        {
            copy_source_client->balance -= req_packet->req.value;
            copy_dest_client->balance += req_packet->req.value;

            copy_source_client->last_req_id = req_packet->seqn;

            req_processed = true;
            // log_request(client_ip_str, dest_ip_str, req_packet->seqn, req_packet->req.value, num_transactions, total_transferred, total_balance);
        }
    }
    if (req_processed)
    {
        pthread_mutex_lock(&read_mutex);
        readers--;
        if (readers == 0)
            pthread_mutex_unlock(&write_mutex);
        pthread_mutex_unlock(&read_mutex);
        pthread_mutex_lock(&write_mutex);
        source_client->last_req_id = copy_source_client.last_req_id;
        source_client->balance = copy_source_client.balance;
        dest_client->balance = copy_dest_client.balance;
        if (!value_was_zero)
        {
            num_transactions++;
            total_transferred += req_packet->req.value;
        };
        log_request(client_ip_str, dest_ip_str, req_packet->seqn, req_packet->req.value, num_transactions, total_transferred, total_balance);
        pthread_mutex_unlock(&write_mutex);
    };

    packet ack_packet;
    memset(&ack_packet, 0, sizeof(ack_packet));
    ack_packet.type = REQ_ACK;
    ack_packet.seqn = req_packet->seqn;
    ack_packet.ack.seqn = source_client->last_req_id;
    ack_packet.ack.new_balance = source_client->balance;

    if (sendto(sockfd, &ack_packet, sizeof(ack_packet), 0, (const struct sockaddr *)client_addr, addr_len) < 0)
    {
        perror("sendto ack");
    }

    printf("\n");
}
