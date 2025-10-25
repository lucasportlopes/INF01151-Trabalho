#include "processing.h"
#include "client.h"
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <strings.h>

#define COMMAND_BUFFER_SIZE 128

typedef struct {
    char line[COMMAND_BUFFER_SIZE];
    bool has_line;
    bool closed;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} command_channel_t;

typedef struct {
    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t server_len;
    char server_ip[INET_ADDRSTRLEN];
    uint32_t next_sequence;
    command_channel_t channel;
    pthread_t reader_thread;
    pthread_t worker_thread;
    volatile sig_atomic_t stop;
} client_context_t;

static volatile sig_atomic_t sigint_received = 0;

static void setup_signal_handler(void);
static void sigint_handler(int signum);

static void command_channel_init(command_channel_t *channel);
static void command_channel_destroy(command_channel_t *channel);
static bool command_channel_push(command_channel_t *channel, const char *line);
static bool command_channel_pop(command_channel_t *channel, char *out_line, size_t out_size);
static void command_channel_close(command_channel_t *channel);

static void trim_newline(char *line);
static bool parse_command(const char *line, char dest_ip[INET_ADDRSTRLEN], uint32_t *value_out);
static bool is_exit_command(const char *line);

static void *input_thread_main(void *arg);
static void *worker_thread_main(void *arg);
static bool handle_transfer_command(client_context_t *ctx, const char *line);

static bool parse_command(const char *line, char dest_ip[INET_ADDRSTRLEN], uint32_t *value_out);

void request(int sockfd, const struct sockaddr_in *server_addr) {
    if (server_addr == NULL) {
        return;
    }

    client_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.sockfd = sockfd;
    ctx.server_addr = *server_addr;
    ctx.server_len = sizeof(*server_addr);
    ctx.next_sequence = 1;
    ctx.stop = 0;

    if (inet_ntop(AF_INET, &ctx.server_addr.sin_addr, ctx.server_ip, sizeof(ctx.server_ip)) == NULL) {
        snprintf(ctx.server_ip, sizeof(ctx.server_ip), "<desconhecido>");
    }

    command_channel_init(&ctx.channel);
    setup_signal_handler();
    sigint_received = 0;

    if (pthread_create(&ctx.worker_thread, NULL, worker_thread_main, &ctx) != 0) {
        perror("pthread_create worker");
        command_channel_destroy(&ctx.channel);
        return;
    }

    if (pthread_create(&ctx.reader_thread, NULL, input_thread_main, &ctx) != 0) {
        perror("pthread_create reader");
        ctx.stop = 1;
        command_channel_close(&ctx.channel);
        pthread_join(ctx.worker_thread, NULL);
        command_channel_destroy(&ctx.channel);
        return;
    }

    pthread_join(ctx.reader_thread, NULL);
    pthread_join(ctx.worker_thread, NULL);

    command_channel_destroy(&ctx.channel);
}

static void setup_signal_handler(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction");
    }
}

static void sigint_handler(int signum) {
    (void)signum;
    sigint_received = 1;
}

static void command_channel_init(command_channel_t *channel) {
    memset(channel->line, 0, sizeof(channel->line));
    channel->has_line = false;
    channel->closed = false;
    pthread_mutex_init(&channel->mutex, NULL);
    pthread_cond_init(&channel->cond, NULL);
}

static void command_channel_destroy(command_channel_t *channel) {
    pthread_cond_destroy(&channel->cond);
    pthread_mutex_destroy(&channel->mutex);
}

static bool command_channel_push(command_channel_t *channel, const char *line) {
    if (line == NULL) {
        return false;
    }

    pthread_mutex_lock(&channel->mutex);
    while (channel->has_line && !channel->closed) {
        pthread_cond_wait(&channel->cond, &channel->mutex);
    }

    if (channel->closed) {
        pthread_mutex_unlock(&channel->mutex);
        return false;
    }

    strncpy(channel->line, line, COMMAND_BUFFER_SIZE - 1);
    channel->line[COMMAND_BUFFER_SIZE - 1] = '\0';
    channel->has_line = true;
    pthread_cond_broadcast(&channel->cond);
    pthread_mutex_unlock(&channel->mutex);

    return true;
}

static bool command_channel_pop(command_channel_t *channel, char *out_line, size_t out_size) {
    if (out_line == NULL || out_size == 0) {
        return false;
    }

    pthread_mutex_lock(&channel->mutex);
    while (!channel->has_line && !channel->closed) {
        pthread_cond_wait(&channel->cond, &channel->mutex);
    }

    if (!channel->has_line) {
        pthread_mutex_unlock(&channel->mutex);
        return false;
    }

    strncpy(out_line, channel->line, out_size - 1);
    out_line[out_size - 1] = '\0';
    channel->has_line = false;
    pthread_cond_broadcast(&channel->cond);
    pthread_mutex_unlock(&channel->mutex);

    return true;
}

static void command_channel_close(command_channel_t *channel) {
    pthread_mutex_lock(&channel->mutex);
    channel->closed = true;
    pthread_cond_broadcast(&channel->cond);
    pthread_mutex_unlock(&channel->mutex);
}

static void trim_newline(char *line) {
    if (line == NULL) {
        return;
    }

    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[len - 1] = '\0';
        len--;
    }
}

static bool is_exit_command(const char *line) {
    if (line == NULL) {
        return false;
    }

    while (*line == ' ' || *line == '\t') {
        line++;
    }

    return strncasecmp(line, "EXIT", 4) == 0;
}

static void *input_thread_main(void *arg) {
    client_context_t *ctx = (client_context_t *)arg;
    char buffer[COMMAND_BUFFER_SIZE];

    while (!ctx->stop) {
        if (sigint_received) {
            ctx->stop = 1;
            break;
        }

        errno = 0;
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            if (feof(stdin)) {
                ctx->stop = 1;
                break;
            }

            if (errno == EINTR && sigint_received) {
                ctx->stop = 1;
                break;
            }

            if (ferror(stdin)) {
                perror("Erro ao ler entrada");
                clearerr(stdin);
            }
            continue;
        }

        trim_newline(buffer);

        if (buffer[0] == '\0') {
            continue;
        }

        if (is_exit_command(buffer)) {
            ctx->stop = 1;
            break;
        }

        if (!command_channel_push(&ctx->channel, buffer)) {
            break;
        }
    }

    command_channel_close(&ctx->channel);
    return NULL;
}

static void *worker_thread_main(void *arg) {
    client_context_t *ctx = (client_context_t *)arg;
    char line[COMMAND_BUFFER_SIZE];

    while (!ctx->stop && command_channel_pop(&ctx->channel, line, sizeof(line))) {
        trim_newline(line);
        if (line[0] == '\0') {
            continue;
        }

        if (!handle_transfer_command(ctx, line)) {
            break;
        }
    }

    ctx->stop = 1;
    command_channel_close(&ctx->channel);
    return NULL;
}

static bool handle_transfer_command(client_context_t *ctx, const char *line) {
    char dest_ip[INET_ADDRSTRLEN];
    uint32_t value = 0;

    if (!parse_command(line, dest_ip, &value)) {
        fprintf(stderr, "Formato invalido. Use: <IP_DESTINO> <VALOR>\n");
        return true;
    }

    struct in_addr dest_addr;
    if (inet_aton(dest_ip, &dest_addr) == 0) {
        fprintf(stderr, "Endereco IP invalido: %s\n", dest_ip);
        return true;
    }

    packet req_pkt;
    memset(&req_pkt, 0, sizeof(req_pkt));
    req_pkt.type = REQ;
    req_pkt.seqn = ctx->next_sequence;
    req_pkt.req.dest_addr = dest_addr.s_addr;
    req_pkt.req.value = value;

    ssize_t sent = sendto(ctx->sockfd, &req_pkt, sizeof(req_pkt), 0,
                          (const struct sockaddr *)&ctx->server_addr, ctx->server_len);

    if (sent < 0) {
        if (errno == EINTR && (ctx->stop || sigint_received)) {
            return false;
        }
        perror("sendto");
        return true;
    }

    printf("[cli] server %s id req %u dest %s value %u (enviada)\n",
           ctx->server_ip, req_pkt.seqn, dest_ip, value);

    packet ack_pkt;
    memset(&ack_pkt, 0, sizeof(ack_pkt));
    struct sockaddr_in reply_addr;
    socklen_t reply_len = sizeof(reply_addr);

    ssize_t received = recvfrom(ctx->sockfd, &ack_pkt, sizeof(ack_pkt), 0,
                                (struct sockaddr *)&reply_addr, &reply_len);

    if (received < 0) {
        if (errno == EINTR && (ctx->stop || sigint_received)) {
            return false;
        }
        perror("recvfrom");
        return true;
    }

    if (ack_pkt.type != REQ_ACK) {
        fprintf(stderr, "[cli] pacote inesperado do servidor (type=%u)\n", (unsigned)ack_pkt.type);
        return true;
    }

    printf("[cli] server %s ack seqn %u new balance %u\n",
           ctx->server_ip, ack_pkt.ack.seqn, ack_pkt.ack.new_balance);

    ctx->next_sequence = ack_pkt.ack.seqn + 1;
    return true;
}

static bool parse_command(const char *line, char dest_ip[INET_ADDRSTRLEN], uint32_t *value_out) {
    if (line == NULL || dest_ip == NULL || value_out == NULL) {
        return false;
    }

    char ip_buffer[INET_ADDRSTRLEN];
    unsigned long value_ul = 0;

    if (sscanf(line, "%15s %lu", ip_buffer, &value_ul) != 2) {
        return false;
    }

    strncpy(dest_ip, ip_buffer, INET_ADDRSTRLEN);
    dest_ip[INET_ADDRSTRLEN - 1] = '\0';

    *value_out = (uint32_t)value_ul;

    return true;
}
