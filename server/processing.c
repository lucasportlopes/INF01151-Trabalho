#include "processing.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void handle_request(int sockfd, const struct sockaddr_in *client_addr, socklen_t addr_len, const packet *req_packet) {
    if (client_addr == NULL || req_packet == NULL) {
        return;
    }

    if (req_packet->type != REQ) {
        fprintf(stderr, "[srv] handle_request: tipo invalido %u\n", (unsigned)req_packet->type);
        return;
    }

    packet ack_packet;
    memset(&ack_packet, 0, sizeof(ack_packet));
    ack_packet.type = REQ_ACK;
    ack_packet.seqn = req_packet->seqn;
    ack_packet.ack.seqn = req_packet->seqn;
    ack_packet.ack.new_balance = 0;

    if (sendto(sockfd, &ack_packet, sizeof(ack_packet), 0, (const struct sockaddr *)client_addr, addr_len) < 0) {
        perror("sendto ack");
    }
}
