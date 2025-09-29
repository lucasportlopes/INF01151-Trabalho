#include "client.h"

int main(int argc, char *argv[]){
    if(argc!=2) {
        printf("Uso: ./cliente <nr_porta>\n");
        return -1;
    }
    int porta = atoi(argv[1]);

    int sock;
    struct sockaddr_in broadcast_addr, recv_addr;
    char buffer[BUF_SIZE];
    socklen_t addr_len = sizeof(recv_addr);

    // Create UDP socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket failed");
        exit(1);
    }

    // Enable broadcast option
    int broadcastEnable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST,
                   &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("setsockopt failed");
        close(sock);
        exit(1);
    }

    // Configure broadcast address
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(porta);
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    // Send broadcast message
    const char *msg = "DISCOVER_SERVER";
    if (sendto(sock, msg, strlen(msg), 0,
               (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
        perror("sendto failed");
        close(sock);
        exit(1);
    }
    printf("Broadcast message sent: %s\n", msg);

    // Receive server reply
    int n = recvfrom(sock, buffer, BUF_SIZE - 1, 0,
                     (struct sockaddr *)&recv_addr, &addr_len);
    if (n < 0) {
        perror("recvfrom failed");
        close(sock);
        exit(1);
    }
    buffer[n] = '\0';

    printf("Received reply from %s: %s\n",
           inet_ntoa(recv_addr.sin_addr), buffer);

    close(sock);
    return 0;
}
