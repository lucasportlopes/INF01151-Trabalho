#ifndef DISCOVERY_H
#define DISCOVERY_H

#include "client.h"

int discover_server(int sockfd, int port, struct sockaddr_in *srv_addr, socklen_t addr_len);

#endif // DISCOVERY_H
