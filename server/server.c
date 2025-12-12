#include "server.h"
#include "discovery.h"
#include "processing.h"
#include "replication.h"

int my_id = -1;
int current_leader_id = -1;
int num_servers = 0;
int num_transactions = 0;
int total_transferred = 0;
int total_balance = 0;
replica_t server_list[MAX_SERVERS];
server_role_t current_role = REPLICA_SECUNDARIO;
pthread_mutex_t data_mutex;

void add_peer(int id, struct sockaddr_in addr)
{
    if (id == my_id)
        return;

    for (int i = 0; i < num_servers; i++)
    {
        if (server_list[i].id == id)
        {
            server_list[i].addr = addr;
            return;
        }
    }

    if (num_servers < MAX_SERVERS)
    {
        server_list[num_servers].id = id;
        server_list[num_servers].addr = addr;
        num_servers++;

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);
        printf("[srv] Novo servidor adicionado na lista: ID %d (%s:%d)\n",
               id, ip, ntohs(addr.sin_port));
    }
    else
    {
        printf("[srv] AVISO: Lista de servidores cheia! Não foi possível adicionar ID %d\n", id);
    }
}

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
    my_id = atoi(argv[2]);
    struct sockaddr_in server_addr;

    // Cria socket UDP
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

    // Pergunta quem é o líder
    packet pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.seqn = my_id;
    pkt.type = FIND_LEADER;
    pkt.send_new.new_server.id = my_id;
    pkt.send_new.new_server.addr = server_addr;

    printf("[STARTUP] Perguntando quem é o líder...\n");

    // Broadcast na rede local
    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    broadcast_addr.sin_port = htons(port);

    // Habilita a permissão de Broadcast no socket
    int broadcast_enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0)
    {
        perror("Erro ao habilitar broadcast");
        return -1;
    }

    /*
    if (sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
        perror("sendto failed");
        close(sockfd);
        return -1;
    }
    */

    // --- GAMBIARRA PARA LOCALHOST (Varre portas 4000 a 4005) ---
    printf("[STARTUP] Procurando líder nas portas vizinhas...\n");

    for (int p = 4000; p <= 4005; p++)
    {
        if (p == port)
            continue;

        struct sockaddr_in target;
        memset(&target, 0, sizeof(target));
        target.sin_family = AF_INET;
        target.sin_port = htons(p);
        target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&target, sizeof(target));
    }
    // -----------------------------------------------------------

    // Timeout de 2 segundos para esperar respostas
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        perror("Falha ao resetar timeout");
    }

    struct sockaddr_in leader_addr;
    socklen_t leader_addr_len = sizeof(leader_addr);
    packet resp_pkt;

    if (recvfrom(sockfd, &resp_pkt, sizeof(packet), 0, (struct sockaddr *)&leader_addr, &leader_addr_len) > 0)
    {
        current_leader_id = resp_pkt.leader.leader_id;
        num_servers = resp_pkt.leader.nr_servers;
        memcpy(server_list, resp_pkt.leader.servers, sizeof(server_list));
        printf("[STARTUP] Líder encontrado: ID %d\n", current_leader_id);

        if (current_leader_id != my_id)
        {
            packet join_pkt;
            memset(&join_pkt, 0, sizeof(join_pkt));
            join_pkt.type = SERVER_JOIN;
            join_pkt.seqn = my_id;
            join_pkt.send_new.new_server.id = my_id;
            join_pkt.send_new.new_server.addr = server_addr;

            // Envia para o endereço de onde veio a resposta do líder
            sendto(sockfd, &join_pkt, sizeof(join_pkt), 0, (struct sockaddr *)&leader_addr, leader_addr_len);
            printf("[STARTUP] Enviei SERVER_JOIN para o Líder ID %d\n", current_leader_id);
        }
    }
    else
    {
        current_leader_id = my_id;
        current_role = REPLICA_PRIMARIO;
        num_servers = 1;
        server_list[0].id = my_id;
        server_list[0].addr = server_addr;
        printf("[STARTUP] Nenhuma resposta. Sou o novo líder.\n");
    }

    log_history(num_transactions, total_transferred, total_balance);

    timeout.tv_sec = 0; 
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // Inicializa o sistema de ping (threads de heartbeat)
    init_ping_system(sockfd);

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
            if (my_id == current_leader_id)
            {
                int backup_id = args->req_packet.send_new.new_server.id;

                add_peer(backup_id, args->client_addr);
                printf("[LIDER] Backup registrado na lista: ID %d\n", backup_id);

                // Responde com COORDINATOR e lista de servidores
                packet resp;
                memset(&resp, 0, sizeof(resp));
                resp.type = COORDINATOR;
                resp.seqn = my_id;
                resp.leader.leader_id = my_id;
                resp.leader.nr_servers = num_servers;
                memcpy(resp.leader.servers, server_list, sizeof(server_list));

                sendto(sockfd, &resp, sizeof(resp), 0, (struct sockaddr *)&args->client_addr, args->addr_len);

                // Envia a tabela para o novo backup
                packet rep_pkt;
                memset(&rep_pkt, 0, sizeof(rep_pkt));
                rep_pkt.type = REP_UPDATE;
                rep_pkt.seqn = num_transactions;
                rep_pkt.rep.num_clients = num_clients;
                rep_pkt.rep.num_transactions = num_transactions;
                rep_pkt.rep.total_transferred = total_transferred;
                rep_pkt.rep.total_balance = total_balance;
                memcpy(rep_pkt.rep.client_table, client_table, sizeof(client_table));

                sendto(sockfd, &rep_pkt, sizeof(rep_pkt), 0, (struct sockaddr *)&args->client_addr, args->addr_len);

                // Informa os outros backups sobre o novo servidor
                for (int i = 0; i < num_servers; i++)
                {
                    if (server_list[i].id == my_id || server_list[i].id == backup_id)
                        continue;

                    sendto(sockfd, &resp, sizeof(resp), 0,
                           (struct sockaddr *)&server_list[i].addr, sizeof(struct sockaddr_in));
                }

                printf("[LIDER] Informei minha liderança para o ID %d\n", args->req_packet.seqn);
            }
            free(args);
            break;

        case SERVER_JOIN:
            if (my_id == current_leader_id)
            {
                int new_id = args->req_packet.send_new.new_server.id;
                struct sockaddr_in new_addr = args->client_addr;
                add_peer(new_id, new_addr);
                printf("[LIDER] Recebi SERVER_JOIN. Backup ID %d adicionado!\n", new_id);
            }
            free(args);
            break;

        case COORDINATOR:
                if ((uint32_t)my_id > args->req_packet.seqn) {
                    printf("[BULLY] Recebi COORDINATOR de ID %d (Sou %d). Não aceito! Iniciando eleição.\n", 
                           args->req_packet.seqn, my_id);
                    
                    start_election(sockfd); 
                } 
                else {
                    current_leader_id = args->req_packet.leader.leader_id;
                    num_servers = args->req_packet.leader.nr_servers;
                    
                    memcpy(server_list, args->req_packet.leader.servers, sizeof(server_list));
                    
                    printf("[INFO] Novo líder reconhecido e aceito: ID %d\n", current_leader_id);
                }
                free(args);
                break;

        case DESC:
            if (my_id == current_leader_id)
            {
                handle_discovery(sockfd, &args->client_addr, args->addr_len);
                printf("[LIDER] Respondi a descoberta de um cliente.\n");
            }
            free(args);
            break;

        case REQ:
            if (my_id == current_leader_id)
            {
                pthread_t thread_id;
                if (pthread_create(&thread_id, NULL, process_request_thread, (void *)args) != 0)
                {
                    perror("pthread_create failed");
                    free(args);
                    continue;
                }
                pthread_detach(thread_id);
            }
            else
            {
                free(args);
            }
            break;

        case REP_UPDATE:
            if (my_id != current_leader_id)
            {
                pthread_mutex_lock(&data_mutex);

                memcpy(client_table, args->req_packet.rep.client_table, sizeof(client_table));
                num_transactions = args->req_packet.rep.num_transactions;
                total_transferred = args->req_packet.rep.total_transferred;
                total_balance = args->req_packet.rep.total_balance;

                printf("[BACKUP] Sincronizado com o Líder (Transação %d)\n", args->req_packet.seqn);
                pthread_mutex_unlock(&data_mutex);
            }
            free(args);
            break;

        case ELECTION:
            if ((int)args->req_packet.seqn < my_id)
            {
                printf("[ELEICAO] Recebi desafio do ID %d (Menor que eu). Respondendo...\n", args->req_packet.seqn);

                packet resp;
                memset(&resp, 0, sizeof(resp));
                resp.type = ELECTION; // Funciona como um "OK"
                resp.seqn = my_id;
                sendto(sockfd, &resp, sizeof(resp), 0, (struct sockaddr *)&args->client_addr, args->addr_len);

                start_election(sockfd);
            }
            else
            {
                printf("[ELEICAO] Recebi desafio de ID %d (Maior). Aguardando ele assumir.\n", args->req_packet.seqn);
            }
            free(args);
            break;

        case PING:
            handle_ping(&args->req_packet, &args->client_addr);
            free(args);
            break;

        default:
            break;
        }
    }

    close(sockfd);
    return 0;
}
