#include "server_discovery.h"

void handle_discovery(int sockfd, struct sockaddr_in *client_addr)
{
    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(*client_addr);

    printf("Received broadcast msg from %s:%d\n", inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));

    socklen_t local_len = sizeof(local_addr);
    if (getsockname(sockfd, (struct sockaddr*)&local_addr, &local_len) == -1) {
        perror("getsockname"); close(sockfd); exit(1);
    }

    char ipstr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local_addr.sin_addr, ipstr, sizeof(ipstr));

    // Send unicast reply back to client
    sendto(sockfd, ipstr, strlen(ipstr), 0, (struct sockaddr *)client_addr, addr_len);

    printf("Sent reply: '%s'\n", ipstr); 
}