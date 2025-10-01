#ifndef SERVER_DISCOVERY_H
#define SERVER_DISCOVERY_H

#include "server.h"

void handle_discovery(int sockfd, struct sockaddr_in *client_addr);

#endif // SERVER_DISCOVERY_H