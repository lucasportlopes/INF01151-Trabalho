#include "discovery.h"

int discover_server(int sockfd, int port, struct sockaddr_in *srv_addr, socklen_t addr_len)
{
    struct sockaddr_in broadcast_addr;
    char server_addr[22];
    char server_ip[INET_ADDRSTRLEN];
    packet msg;

    // Configure broadcast address
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(port);
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    // Send broadcast message
    msg.type = DESC;
    msg.seqn = 0;

    struct timeval timeout;
    timeout.tv_sec = 2; // 0 seconds
    timeout.tv_usec = 0; // 10000 microseconds

    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("send timeout failed");
        close(sockfd);
        exit(1);
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("recv timeout failed");
        close(sockfd);
        exit(1);
    }

    // Retry loop until we get a server response
    while (1) {
        // Send broadcast message
        if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("sendto failed");
                close(sockfd);
                exit(1);
            }
        }

        if(recvfrom(sockfd, server_ip, INET_ADDRSTRLEN, 0, (struct sockaddr *)&srv_addr, &addr_len) < 0){
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout
                log_timeout();
                continue;   // retry sending broadcast
            } else {
                perror("recvfrom failed");
                close(sockfd);
                exit(1);
            }
        }

        // Got a reply
        sprintf(server_addr, "%s:%d", server_ip, port);
        log_connection(server_addr);

        break;  // stop retrying
    }

    return 0;
}
