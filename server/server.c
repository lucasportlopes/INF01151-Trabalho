#include "server.h"
#include "discovery.h"
#include "processing.h"

int my_id = -1;
int current_leader_id = -1;
int num_servers = 0;
replica_t server_list[MAX_SERVERS];
int num_transactions = 0;
int total_transferred = 0;
int total_balance = 0;

pthread_mutex_t data_mutex;

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Uso: ./servidor <nr_porta> <ID>\n");
        return -1;
    }

    if (pthread_mutex_init(&data_mutex, NULL) != 0)
    {
        perror("Mutex init failed");
        return -1;
    }

    int port = atoi(argv[1]), sockfd;
    int my_id = atoi(argv[2]); // Lemos o ID do argumento

    struct sockaddr_in server_addr;
    // socklen_t addr_len = sizeof(client_addr);
    // packet msg;

    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket failed");
        exit(1);
    }

    // Bind to all interfaces
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind failed");
        close(sockfd);
        exit(1);
    }

    // Start Ask who is the leader

    packet_servers pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.seqn = my_id;
    pkt.type = FIND_LEADER;
    pkt.send_info.new_server.id = my_id;
    pkt.send_info.new_server.addr = server_addr;

    printf("[STARTUP] Perguntando quem é o líder...\n");
    // Broadcast to local network

    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST); // Endereço universal de broadcast
    broadcast_addr.sin_port = htons(port); // Envia para a porta dos servidores

    // 2. Habilita a permissão de Broadcast no socket
    int broadcast_enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        perror("Erro ao habilitar broadcast");
        return;
    }

    if (sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
        perror("sendto failed");
        close(sockfd);
        return;
    }

    // Wait for response with timeout
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in leader_addr;
    socklen_t leader_addr_len = sizeof(leader_addr);
    packet_servers resp_pkt;

    if (recvfrom(sockfd, &resp_pkt, sizeof(packet_servers), 0, (struct sockaddr *)&leader_addr, &leader_addr_len) > 0) {
        current_leader_id = resp_pkt.leader.leader_id;
        num_servers = resp_pkt.leader.nr_servers;
        memcpy(server_list, resp_pkt.leader.servers, sizeof(server_list));
        printf("[STARTUP] Líder encontrado: ID %d\n", current_leader_id);
    } else {
        current_leader_id = my_id;
        num_servers = 1;
        server_list[0].id = my_id;
        server_list[0].addr = server_addr;
        printf("[STARTUP] Nenhuma resposta. Sou o novo líder.\n");
    }

    // End Ask who is the leader

    // printf("Server listening on 0.0.0.0:%d...\n", port);

    int num_transactions = 0, total_transferred = 0, total_balance = 0;
    log_history(num_transactions, total_transferred, total_balance);

    while (1)
    {
        thread_args_t *args = malloc(sizeof(thread_args_t));
        if (args == NULL)
        {
            perror("malloc failed");
            continue;
        }

        args->addr_len = sizeof(args->client_addr);
        if (recvfrom(sockfd, &args->req_packet, sizeof(packet), 0, (struct sockaddr *)&args->client_addr, &args->addr_len) < 0)
        {
            perror("recvfrom failed");
            free(args);
            continue;
        }

        args->sockfd = sockfd;

        switch (args->req_packet.type)
        {
        case FIND_LEADER:
            // Só respondo se EU sou o líder atual
            if (my_id == current_leader_id)
            {
                packet resp;
                memset(&resp, 0, sizeof(resp));
                resp.type = COORDINATOR; // "Eu sou o coordenador"
                resp.seqn = my_id;       // Meu ID para confirmar

                // Envia de volta para quem perguntou
                sendto(sockfd, &resp, sizeof(resp), 0,
                       (struct sockaddr *)&args->client_addr, args->addr_len);

                printf("[LIDER] Informei minha liderança para o ID %d\n", args->req_packet.seqn);
            }
            free(args);
            break;

        case COORDINATOR:
            // Recebi a resposta! Atualizo meu estado.
            current_leader_id = args->req_packet.seqn;
            printf("[INFO] Novo líder reconhecido: ID %d\n", current_leader_id);

            // Se eu achava que era lider e recebi isso de alguém com ID maior, deixo de ser
            // Mas na lógica de startup, apenas anoto quem é o chefe.
            free(args);
            break;
        case DESC:
            handle_discovery(sockfd, &args->client_addr, args->addr_len);
            free(args);
            break;
        case REQ:
            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, process_request_thread, (void *)args) != 0)
            {
                perror("pthread_create failed");
                free(args);
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
