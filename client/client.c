#include "client.h"
#include "client_discovery.h"

int main(int argc, char *argv[]){
    if(argc!=2) {
        printf("Uso: ./cliente <nr_porta>\n");
        return -1;
    }
    int porta = atoi(argv[1]);

    int sock;
    struct sockaddr_in srv_addr;

    // Create UDP socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket failed");
        exit(1);
    }

    // Discover server
    if(discover_server(sock, porta, &srv_addr) != 0) {
        printf("Server not found\n");
        close(sock);
        return -1;
    }

    printf("Server found at %s:%d\n", inet_ntoa(srv_addr.sin_addr), ntohs(srv_addr.sin_port));

    return 0;
}
