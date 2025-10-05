#ifndef PROCESSING_H
#define PROCESSING_H

#include <netinet/in.h>

void request(int sockfd, const struct sockaddr_in *server_addr);

#endif // PROCESSING_H