#ifndef DISCOVERY_H
#define DISCOVERY_H

#include "server.h"


void handle_discovery(int sockfd, struct sockaddr_in *client_addr, socklen_t addr_len);

#endif // DISCOVERY_H
