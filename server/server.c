#include "server.h"
#include "discovery.h"
#include "processing.h"

int num_transactions = 0;
int total_transferred = 0;
int total_balance = 0;

int main(int argc, char *argv[]) {
    if(argc!=2) {
        printf("Uso: ./servidor <nr_porta>\n");
        return -1;
    }

    int port = atoi(argv[1]), sockfd;
    struct sockaddr_in server_addr;
    //socklen_t addr_len = sizeof(client_addr);
    //packet msg;

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
        thread_args_t *args = malloc(sizeof(thread_args_t));
        if (args == NULL) {
            perror("malloc failed");
            continue;
        }

        args->addr_len = sizeof(args->client_addr);
        if (recvfrom(sockfd, &args->req_packet, sizeof(packet), 0, (struct sockaddr *)&args->client_addr, &args->addr_len) < 0) {
            perror("recvfrom failed");
            free(args); // Não se esqueça de libertar a memória em caso de erro
            continue;
        }

        // Passar uma cópia do descritor do socket para a thread
        args->sockfd = sockfd;

        switch (args->req_packet.type) {
            case DESC:
                handle_discovery(sockfd, &args->client_addr, args->addr_len);
                free(args);
                break;
            case REQ:
                pthread_t thread_id;
                if (pthread_create(&thread_id, NULL, process_request_thread, (void*)args) != 0) {
                    perror("pthread_create failed");
                    free(args); // Liberta a memória em caso de falha na criação da thread
                    continue;
                }
                pthread_detach(thread_id);
                break;
            default:
                break;
        }
    }

    close(sockfd);
    return 0;
}
