#include "discovery.h"
#include "processing.h"

void handle_discovery(int sockfd, struct sockaddr_in *client_addr, socklen_t addr_len)
{
    struct sockaddr_in local_addr;
    socklen_t local_len = sizeof(local_addr);
    char client_ip[22];
    char server_ip[INET_ADDRSTRLEN];

    if (getsockname(sockfd, (struct sockaddr*)&local_addr, &local_len) == -1) {
        perror("getsockname");
        close(sockfd);
        exit(1);
    }

    inet_ntop(AF_INET, &local_addr.sin_addr, server_ip, sizeof(server_ip));

    // Add client to the list (if not already present)
    find_or_add_client(client_addr->sin_addr.s_addr);

    // Send unicast reply back to client
    if (sendto(sockfd, server_ip, INET_ADDRSTRLEN, 0, (struct sockaddr *)client_addr, addr_len) < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("sendto failed");
            close(sockfd);
            exit(1);
        }
    }
    // printf("Sent reply: '%s'\n", server_ip);

    sprintf(client_ip, "%s:%d", inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
    log_discovery(client_ip);
}
