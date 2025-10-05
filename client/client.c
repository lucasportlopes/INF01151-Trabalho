#include "client.h"
#include "discovery.h"
#include "processing.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: ./cliente <nr_porta>\n");
        return -1;
    }

    int port = atoi(argv[1]);

    int sockfd;
    struct sockaddr_in srv_addr;
    socklen_t addr_len = sizeof(srv_addr);

    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket failed");
        exit(1);
    }

    // Enable broadcast option
    int broadcastEnable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("setsockopt failed");
        close(sockfd);
        exit(1);
    }

    // Discover server
    if (discover_server(sockfd, port, &srv_addr, addr_len) != 0) {
        perror("Server not found\n");
        close(sockfd);
        exit(1);
    }

    request(sockfd, &srv_addr);

    close(sockfd);
    return 0;
}
