#ifndef PROCESSING_H
#define PROCESSING_H

#include "server.h"
#include <netinet/in.h>
#include <sys/socket.h>

void handle_request(int sockfd, const struct sockaddr_in *client_addr, socklen_t addr_len, const packet *req_packet);

#endif // PROCESSING_H