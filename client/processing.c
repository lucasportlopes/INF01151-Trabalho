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

#define COMMAND_BUFFER_SIZE 128

static bool parse_command(const char *line, char dest_ip[INET_ADDRSTRLEN], uint32_t *value_out);

void request(int sockfd, const struct sockaddr_in *server_addr) {
    if (server_addr == NULL) {
        return;
    }

    struct sockaddr_in srv_addr = *server_addr;
    socklen_t srv_len = sizeof(srv_addr);

    char server_ip[INET_ADDRSTRLEN];

    if (inet_ntop(AF_INET, &srv_addr.sin_addr, server_ip, sizeof(server_ip)) == NULL) {
        perror("inet_ntop");
    }

    char input_line[COMMAND_BUFFER_SIZE];
    uint32_t next_sequence = 1;

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

        char dest_ip[INET_ADDRSTRLEN];

        uint32_t value = 0;
        
        if (!parse_command(input_line, dest_ip, &value)) {
            fprintf(stderr, "Formato de entrada invalido. Use: <IP_DESTINO> <VALOR>\n");
            continue;
        }

        struct in_addr dest_addr;

        if (inet_aton(dest_ip, &dest_addr) == 0) {
            fprintf(stderr, "Endereco IP invalido: %s\n", dest_ip);
            continue;
        }

        packet req_pkt;
        memset(&req_pkt, 0, sizeof(req_pkt));
        req_pkt.type = REQ;
        req_pkt.seqn = next_sequence;
        req_pkt.req.dest_addr = dest_addr.s_addr;
        req_pkt.req.value = value;

        ssize_t sent = sendto(sockfd, &req_pkt, sizeof(req_pkt), 0, (const struct sockaddr *)&srv_addr, srv_len);

        if (sent < 0) {
            perror("sendto");
            continue;
        }

        printf("[cli] server %s id req %u dest %s value %u (enviada)\n", server_ip, req_pkt.seqn, dest_ip, value);

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