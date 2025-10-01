#include "client_discovery.h"

int discover_server(int sock, int port, struct sockaddr_in *srv_addr)
{
    struct sockaddr_in broadcast_addr;
    char buffer[BUF_SIZE];
    socklen_t addr_len = sizeof(*srv_addr);

    // Enable broadcast option
    int broadcastEnable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST,
                   &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("setsockopt failed");
        return -1;
    }

    // Configure broadcast address
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(port);
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    packet msg;
    msg.type = DESC;
    msg.seqn = 0;

    if (sendto(sock, &msg, sizeof(msg), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
        perror("sendto failed");
        return -1;
    }
    printf("Broadcast discovery msg sent");

    int n = recvfrom(sock, buffer, BUF_SIZE - 1, 0, (struct sockaddr *)srv_addr, &addr_len);
    if (n < 0) {
        perror("recvfrom failed");
        return -1;
    }

    buffer[n] = '\0';

    printf("Received reply from %s: %s\n", inet_ntoa(srv_addr->sin_addr), buffer);

    return 0;
}