#ifndef PROCESSING_H
#define PROCESSING_H

#include "server.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>

typedef struct {
    uint32_t ip_addr;
    uint32_t last_req_id;
    int balance;
    int in_use; 
} client_t;

void* process_request_thread(void* args);
client_t* find_or_add_client(uint32_t ip_addr);
client_t* find_client_by_ip(uint32_t ip_addr);
void handle_request(int sockfd, const struct sockaddr_in *client_addr, socklen_t addr_len, const packet *req_packet);

#endif // PROCESSING_H