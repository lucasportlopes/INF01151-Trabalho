#include "server.h"
#include "discovery.h"

int main(int argc, char *argv[]) {
    if(argc!=2) {
        printf("Uso: ./servidor <nr_porta>\n");
        return -1;
    }

    int port = atoi(argv[1]), sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    packet msg;

    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket failed");
        exit(1);
    }

    // Bind to all interfaces
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(sockfd);
        exit(1);
    }

    // printf("Server listening on 0.0.0.0:%d...\n", port);

    int num_transactions = 0, total_transferred = 0, total_balance = 0;
    log_history(num_transactions, total_transferred, total_balance);

    while (1) {
        if (recvfrom(sockfd, &msg, sizeof(packet), 0, (struct sockaddr *)&client_addr, &addr_len) < 0) {
            perror("recvfrom failed");
            continue;
        }

        if (msg.type == DESC) {
            handle_discovery(sockfd, &client_addr, addr_len);
        }
    }

    close(sockfd);
    return 0;
}
