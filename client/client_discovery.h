#ifndef CLIENT_DISCOVERY_H
#define CLIENT_DISCOVERY_H

#include "client.h"

int discover_server(int sock, int port, struct sockaddr_in *srv_addr);

#endif // CLIENT_DISCOVERY_H