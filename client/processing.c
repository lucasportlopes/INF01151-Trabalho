#include "processing.h"
#include "client.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>

#define COMMAND_BUFFER_SIZE 128
#define QUEUE_SIZE 100

// Estrutura para fila de comandos FIFO
typedef struct {
    struct requisicao commands[QUEUE_SIZE]; // Array circular de comandos
    int front; // Aponta para o próximo item a ser removido
    int rear; // Aponta para onde inserir o próximo item
    int count; // Quantidade de elementos na fila
    pthread_mutex_t mutex; // Mutex para proteger o acesso à fila
    pthread_cond_t not_empty; // Condição para fila não vazia
    pthread_cond_t not_full; // Condição para fila não cheia
    bool shutdown; // Indica se deve encerrar
} command_queue_t;

typedef struct {
    int sockfd;
    struct sockaddr_in server_addr;
    command_queue_t *queue;
} thread_data_t;

static bool parse_command(const char *line, struct requisicao *req_out);
static void queue_init(command_queue_t *q);
static void queue_destroy(command_queue_t *q);
static bool queue_push(command_queue_t *q, const struct requisicao *req);
static bool queue_pop(command_queue_t *q, struct requisicao *req);
static void* reader_thread(void *arg);
static void* sender_thread(void *arg);

void request(int sockfd, const struct sockaddr_in *server_addr) {
    if (server_addr == NULL) {
        return;
    }

    command_queue_t queue;
    queue_init(&queue);

    thread_data_t data = {
        .sockfd = sockfd,
        .server_addr = *server_addr,
        .queue = &queue
    };

    pthread_t reader_tid, sender_tid;

    if (pthread_create(&reader_tid, NULL, reader_thread, &data) != 0) {
        perror("pthread_create (reader)");
        queue_destroy(&queue);
        return;
    }

    if (pthread_create(&sender_tid, NULL, sender_thread, &data) != 0) {
        perror("pthread_create (sender)");
        queue.shutdown = true;
        pthread_cond_signal(&queue.not_empty);
        pthread_join(reader_tid, NULL);
        queue_destroy(&queue);
        return;
    }

    pthread_join(reader_tid, NULL);
    pthread_join(sender_tid, NULL);

    queue_destroy(&queue);
}

static void* reader_thread(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    command_queue_t *queue = data->queue;
    char input_line[COMMAND_BUFFER_SIZE];

    while (true) {
        if (fgets(input_line, sizeof(input_line), stdin) == NULL) {
            if (feof(stdin)) {
                break;
            }

            if (ferror(stdin)) {
                perror("Ocorreu um erro ao ler a entrada!\n");
            }
            
            clearerr(stdin);
            continue;
        }

        struct requisicao req;
        
        if (!parse_command(input_line, &req)) {
            fprintf(stderr, "Formato de entrada invalido. Use: <IP_DESTINO> <VALOR>\n");
            continue;
        }

        if (!queue_push(queue, &req)) {
            fprintf(stderr, "Fila de comandos cheia. Aguarde...\n");
            queue_push(queue, &req); // Tenta novamente com bloqueio
        }
    }

    pthread_mutex_lock(&queue->mutex);
    queue->shutdown = true;
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);

    return NULL;
}

static void* sender_thread(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    int sockfd = data->sockfd;
    struct sockaddr_in srv_addr = data->server_addr;
    socklen_t srv_len = sizeof(srv_addr);
    command_queue_t *queue = data->queue;

    char server_ip[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &srv_addr.sin_addr, server_ip, sizeof(server_ip)) == NULL) {
        perror("inet_ntop");
    }

    uint32_t next_sequence = 1;

    while (true) {
        struct requisicao req;
        
        if (!queue_pop(queue, &req)) {
            break; // shutdown
        }

        packet req_pkt;
        memset(&req_pkt, 0, sizeof(req_pkt));
        req_pkt.type = REQ;
        req_pkt.seqn = next_sequence;
        req_pkt.req = req; // Usa a struct requisicao diretamente

        ssize_t sent = sendto(sockfd, &req_pkt, sizeof(req_pkt), 0, (const struct sockaddr *)&srv_addr, srv_len);

        if (sent < 0) {
            perror("sendto");
            continue;
        }

        char dest_ip[INET_ADDRSTRLEN];
        struct in_addr dest_addr;
        dest_addr.s_addr = req.dest_addr;
        inet_ntop(AF_INET, &dest_addr, dest_ip, sizeof(dest_ip));

        printf("[cli] server %s id req %u dest %s value %u (enviada)\n", server_ip, req_pkt.seqn, dest_ip, req.value);

        packet ack_pkt;
        memset(&ack_pkt, 0, sizeof(ack_pkt));
        struct sockaddr_in reply_addr;
        socklen_t reply_len = sizeof(reply_addr);

        ssize_t received = recvfrom(sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&reply_addr, &reply_len);

        if (received < 0) {
            perror("recvfrom");
            continue;
        }

        if (ack_pkt.type != REQ_ACK) {
            fprintf(stderr, "[cli] pacote inesperado do servidor (type=%u)\n", (unsigned)ack_pkt.type);
            continue;
        }

        printf("[cli] server %s ack seqn %u new balance %u\n", server_ip, ack_pkt.ack.seqn, ack_pkt.ack.new_balance);

        next_sequence = ack_pkt.ack.seqn + 1;
    }

    return NULL;
}

static void queue_init(command_queue_t *q) {
    q->front = 0;
    q->rear = 0;
    q->count = 0;
    q->shutdown = false;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

static void queue_destroy(command_queue_t *q) {
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

static bool queue_push(command_queue_t *q, const struct requisicao *req) {
    pthread_mutex_lock(&q->mutex);

    while (q->count >= QUEUE_SIZE && !q->shutdown) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }

    if (q->shutdown) {
        pthread_mutex_unlock(&q->mutex);
        return false;
    }

    q->commands[q->rear] = *req;
    q->rear = (q->rear + 1) % QUEUE_SIZE;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);

    return true;
}

static bool queue_pop(command_queue_t *q, struct requisicao *req) {
    pthread_mutex_lock(&q->mutex);

    while (q->count == 0 && !q->shutdown) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }

    if (q->count == 0 && q->shutdown) {
        pthread_mutex_unlock(&q->mutex);
        return false;
    }

    *req = q->commands[q->front];
    q->front = (q->front + 1) % QUEUE_SIZE;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);

    return true;
}

static bool parse_command(const char *line, struct requisicao *req_out) {
    if (line == NULL || req_out == NULL) {
        return false;
    }

    char ip_buffer[INET_ADDRSTRLEN];
    unsigned long value_ul = 0;

    if (sscanf(line, "%15s %lu", ip_buffer, &value_ul) != 2) {
        return false;
    }

    struct in_addr dest_addr;

    if (inet_aton(ip_buffer, &dest_addr) == 0) {
        return false;
    }

    req_out->dest_addr = dest_addr.s_addr;
    req_out->value = (uint32_t)value_ul;

    return true;
}