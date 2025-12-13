#include "server.h"
#include "processing.h"
#include "replication.h"
#include <time.h>
#include <string.h>
#include <stdio.h>

// Declarações externas
extern client_t client_table[];
extern int num_clients;
extern int num_transactions;
extern int total_transferred;
extern int total_balance;
extern pthread_mutex_t data_mutex;
extern int my_id;
extern int current_leader_id;
extern int num_servers;
extern replica_t server_list[];
extern server_role_t current_role;

// Variáveis locais para controle de eleição
static int election_in_progress = 0;
static pthread_mutex_t election_mutex = PTHREAD_MUTEX_INITIALIZER;

// Variáveis para controle de ping/heartbeat
typedef struct {
    time_t last_ping_received;
    int is_alive;
} server_health_t;

static server_health_t server_health[MAX_SERVERS];
static pthread_mutex_t health_mutex = PTHREAD_MUTEX_INITIALIZER;

#define PING_INTERVAL 2      // Envia ping a cada 2 segundos
#define PING_TIMEOUT 6       // Considera morto após 6 segundos sem resposta
#define FAILURE_THRESHOLD 10 // Aguarda 10 segundos antes de iniciar eleição

/**
 * Inicia processo de eleição (Algoritmo do Valentão)
 * @param sockfd Socket para comunicação
 */
void start_election(int sockfd) {
    printf("[ELECTION] =======================================\n");
    printf("[ELECTION] Iniciando eleição - Meu ID: %d\n", my_id);
    printf("[ELECTION] =======================================\n");
    
    // Marca que eleição está em andamento
    pthread_mutex_lock(&election_mutex);
    election_in_progress = 1;
    pthread_mutex_unlock(&election_mutex);
    
    // Envia mensagem ELECTION para todos os servidores com ID maior
    packet election_pkt;
    memset(&election_pkt, 0, sizeof(election_pkt));
    election_pkt.type = ELECTION;
    election_pkt.leader.leader_id = my_id;
    
    int sent_to_higher = 0;
    
    printf("[ELECTION] Servidores conhecidos: %d\n", num_servers);
    for (int i = 0; i < num_servers; i++) {
        printf("[ELECTION] - Servidor ID %d\n", server_list[i].id);
        if (server_list[i].id > my_id) {
            ssize_t sent = sendto(sockfd, &election_pkt, sizeof(election_pkt), 0,
                                 (struct sockaddr*)&server_list[i].addr,
                                 sizeof(server_list[i].addr));
            
            if (sent > 0) {
                printf("[ELECTION] Mensagem ELECTION enviada para servidor ID: %d\n", 
                       server_list[i].id);
                sent_to_higher++;
            } else {
                perror("[ELECTION] Erro ao enviar ELECTION");
            }
        }
    }
    
    // Se não há servidores com ID maior, este processo se torna o coordenador
    if (sent_to_higher == 0) {
        printf("[ELECTION] Nenhum servidor com ID maior encontrado.\n");
        printf("[ELECTION] Tornando-me COORDENADOR!\n");
        sleep(1); // Pequeno delay para garantir que não há outros processos
        promote_to_primary(sockfd);
        return;
    }
    
    // Aguarda respostas por um timeout
    printf("[ELECTION] Aguardando respostas de %d servidor(es) com ID maior...\n", sent_to_higher);
    sleep(3); // Timeout de 3 segundos
    
    // Se não recebeu nenhuma resposta, torna-se coordenador
    pthread_mutex_lock(&election_mutex);
    int still_in_election = election_in_progress;
    pthread_mutex_unlock(&election_mutex);
    
    if (still_in_election) {
        printf("[ELECTION] Timeout! Nenhuma resposta recebida.\n");
        printf("[ELECTION] Tornando-me COORDENADOR!\n");
        promote_to_primary(sockfd);
    } else {
        printf("[ELECTION] Resposta recebida. Aguardando novo coordenador...\n");
    }
}

/**
 * Processa mensagem ELECTION
 * @param sockfd Socket para comunicação
 * @param pkt Pacote recebido
 * @param sender_addr Endereço do remetente
 */
void handle_election_message(int sockfd, const packet* pkt, const struct sockaddr_in* sender_addr) {
    if (pkt == NULL || sender_addr == NULL) {
        return;
    }
    
    int sender_id = pkt->leader.leader_id;
    printf("[ELECTION] =======================================\n");
    printf("[ELECTION] Mensagem ELECTION recebida do servidor ID: %d\n", sender_id);
    printf("[ELECTION] Meu ID: %d\n", my_id);
    printf("[ELECTION] =======================================\n");
    
    // Como este processo tem ID maior (senão não receberia ELECTION), envia resposta
    packet answer_pkt;
    memset(&answer_pkt, 0, sizeof(answer_pkt));
    answer_pkt.type = ANSWER; // Usando ANSWER como resposta 
    answer_pkt.leader.leader_id = my_id;
    
    sendto(sockfd, &answer_pkt, sizeof(answer_pkt), 0,
           (struct sockaddr*)sender_addr, sizeof(*sender_addr));
    
    printf("[ELECTION] ANSWER enviado para servidor ID: %d\n", sender_id);
    
    // Inicia própria eleição se ainda não estiver em uma
    pthread_mutex_lock(&election_mutex);
    int already_in_election = election_in_progress;
    pthread_mutex_unlock(&election_mutex);
    
    if (!already_in_election) {
        printf("[ELECTION] Iniciando minha própria eleição em resposta...\n");
        start_election(sockfd);
    } else {
        printf("[ELECTION] Já estou em processo de eleição.\n");
    }
}

/**
 * Processa mensagem ANSWER (resposta à eleição)
 * @param pkt Pacote recebido
 */
void handle_answer_message(const packet* pkt) {
    if (pkt == NULL) {
        return;
    }
    
    int sender_id = pkt->leader.leader_id;
    printf("[ELECTION] =======================================\n");
    printf("[ELECTION] ANSWER recebido do servidor ID: %d\n", sender_id);
    printf("[ELECTION] =======================================\n");
    
    // Recebeu resposta de um processo com ID maior, então cancela sua candidatura
    pthread_mutex_lock(&election_mutex);
    election_in_progress = 0;
    pthread_mutex_unlock(&election_mutex);
    
    printf("[ELECTION] Cancelando candidatura (servidor com ID maior respondeu)\n");
    printf("[ELECTION] Aguardando anúncio do novo coordenador...\n");
}

/**
 * Processa mensagem COORDINATOR (anúncio de novo líder)
 * @param pkt Pacote recebido
 */
void handle_coordinator_message(const packet* pkt) {
    if (pkt == NULL) {
        return;
    }
    
    int new_leader_id = pkt->leader.leader_id;
    printf("[ELECTION] =======================================\n");
    printf("[ELECTION] Novo COORDENADOR anunciado: ID %d\n", new_leader_id);
    printf("[ELECTION] =======================================\n");
    
    // Atualiza informações do líder
    current_leader_id = new_leader_id;

    pthread_mutex_lock(&health_mutex);
    for(int i=0; i<num_servers; i++) {
        if(server_list[i].id == new_leader_id) {
            server_health[i].last_ping_received = time(NULL);
            server_health[i].is_alive = 1;
        }
    }
    pthread_mutex_unlock(&health_mutex);
    
    pthread_mutex_lock(&election_mutex);
    election_in_progress = 0;
    pthread_mutex_unlock(&election_mutex);
    
    // Se não sou o líder, mudo meu papel para secundário
    if (my_id != new_leader_id) {
        current_role = REPLICA_SECUNDARIO;
        printf("[ELECTION] Meu novo papel: SECUNDÁRIO (BACKUP)\n");
    } else {
        current_role = REPLICA_PRIMARIO;
        printf("[ELECTION] Meu novo papel: PRIMÁRIO (LÍDER)\n");
    }
    
    // Atualiza lista de servidores se fornecida
    if (pkt->leader.nr_servers > 0) {
        num_servers = pkt->leader.nr_servers;
        for (int i = 0; i < num_servers && i < MAX_SERVERS; i++) {
            server_list[i] = pkt->leader.servers[i];
        }
        printf("[ELECTION] Lista de servidores atualizada (%d servidores)\n", num_servers);
    }
    
    printf("[ELECTION] Eleição finalizada. Líder atual: ID %d\n", current_leader_id);
}

/**
 * Promove este servidor de backup para primário
 * @param sockfd Socket para comunicação
 */
void promote_to_primary(int sockfd) {
    printf("[ELECTION] =======================================\n");
    printf("[ELECTION]     PROMOÇÃO PARA PRIMÁRIO\n");
    printf("[ELECTION]           ID: %d\n", my_id);
    printf("[ELECTION] =======================================\n");
    
    // Atualiza papel e líder
    current_role = REPLICA_PRIMARIO;
    current_leader_id = my_id;
    
    pthread_mutex_lock(&election_mutex);
    election_in_progress = 0;
    pthread_mutex_unlock(&election_mutex);

    pthread_mutex_lock(&health_mutex);
    for(int i=0; i<MAX_SERVERS; i++) {
        server_health[i].is_alive = 1;
        server_health[i].last_ping_received = time(NULL);
    }
    pthread_mutex_unlock(&health_mutex);
    
    // Anuncia coordenação para todos os outros servidores
    packet coordinator_pkt;
    memset(&coordinator_pkt, 0, sizeof(coordinator_pkt));
    coordinator_pkt.type = COORDINATOR;
    coordinator_pkt.leader.leader_id = my_id;
    coordinator_pkt.leader.nr_servers = num_servers;
    
    // Copia lista de servidores
    for (int i = 0; i < num_servers && i < MAX_SERVERS; i++) {
        coordinator_pkt.leader.servers[i] = server_list[i];
    }
    
    printf("[ELECTION] Enviando anúncio COORDINATOR para todos os servidores...\n");
    
    // Envia para todos os outros servidores
    int sent_count = 0;
    for (int i = 0; i < num_servers; i++) {
        if (server_list[i].id != my_id) {
            ssize_t sent = sendto(sockfd, &coordinator_pkt, sizeof(coordinator_pkt), 0,
                                 (struct sockaddr*)&server_list[i].addr,
                                 sizeof(server_list[i].addr));
            
            if (sent > 0) {
                printf("[ELECTION] COORDINATOR enviado para servidor ID: %d\n", 
                       server_list[i].id);
                sent_count++;
            } else {
                perror("[ELECTION] Erro ao enviar COORDINATOR");
            }
        }
    }
    
    printf("[ELECTION] =======================================\n");
    printf("[ELECTION] Agora sou o COORDENADOR da rede!\n");
    printf("[ELECTION] Anúncios enviados: %d\n", sent_count);
    printf("[ELECTION] Pronto para processar requisições\n");
    printf("[ELECTION] =======================================\n");

    pthread_mutex_lock(&data_mutex);
    propagate_state_to_backups(sockfd);
    pthread_mutex_unlock(&data_mutex);
}

/**
 * Propaga estado atualizado para os backups
 * @param sockfd Socket para comunicação
 */
void propagate_state_to_backups(int sockfd) {
    // Apenas o primário propaga estado
    if (current_role != REPLICA_PRIMARIO) {
        return;
    }
    
    packet rep_pkt;
    memset(&rep_pkt, 0, sizeof(rep_pkt));
    rep_pkt.type = REP_UPDATE;
    
    // Preenche mensagem de replicação
    rep_pkt.seqn = num_transactions;
    rep_pkt.rep.num_clients = num_clients;
    rep_pkt.rep.num_transactions = num_transactions;
    rep_pkt.rep.total_transferred = total_transferred;
    rep_pkt.rep.total_balance = total_balance;
    
    // Copia tabela de clientes
    for (int i = 0; i < MAX_CLIENTS; i++) {
        rep_pkt.rep.client_table[i] = client_table[i];
    }
    
    // Envia para todos os backups
    int sent_count = 0;
    for (int i = 0; i < num_servers; i++) {
        if (server_list[i].id != my_id) {
            ssize_t sent = sendto(sockfd, &rep_pkt, sizeof(rep_pkt), 0,
                                 (struct sockaddr*)&server_list[i].addr,
                                 sizeof(server_list[i].addr));
            
            if (sent > 0) {
                sent_count++;
            }
        }
    }
    
    if (sent_count > 0) {
        printf("[REPLICATION] Estado propagado para %d backup(s)\n", sent_count);
    }
}

/**
 * Aplica atualização de estado recebida (usado por backups)
 * @param rep Mensagem de replicação
 */
void apply_state_update(const struct replication_msg* rep) {
    if (rep == NULL) {
        return;
    }
    
    // Apenas backups aplicam atualizações
    if (current_role != REPLICA_SECUNDARIO) {
        return;
    }
    
    pthread_mutex_lock(&data_mutex);
    
    // Atualiza estatísticas
    num_clients = rep->num_clients;
    num_transactions = rep->num_transactions;
    total_transferred = rep->total_transferred;
    total_balance = rep->total_balance;
    
    // Atualiza tabela de clientes
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_table[i] = rep->client_table[i];
    }
    
    pthread_mutex_unlock(&data_mutex);
    
    printf("[REPLICATION] Estado atualizado - Trans: %d, Transferido: %d, Saldo: %d\n",
           num_transactions, total_transferred, total_balance);
}

/**
 * Processa mensagem REP_UPDATE recebida
 * @param pkt Pacote recebido
 * @param sockfd Socket para enviar confirmação
 * @param sender_addr Endereço do remetente
 */
void handle_replication_update(const packet* pkt, int sockfd, const struct sockaddr_in* sender_addr) {
    if (pkt == NULL || sender_addr == NULL) {
        return;
    }
    
    // Aplica atualização
    apply_state_update(&pkt->rep);
    
    // Envia confirmação
    packet confirm_pkt;
    memset(&confirm_pkt, 0, sizeof(confirm_pkt));
    confirm_pkt.type = REP_CONFIRM;
    confirm_pkt.leader.leader_id = my_id;
    
    sendto(sockfd, &confirm_pkt, sizeof(confirm_pkt), 0,
           (struct sockaddr*)sender_addr, sizeof(*sender_addr));
}

/**
 * Thread que envia pings periodicamente para todos os servidores
 * @param arg Ponteiro para o socket file descriptor (int*)
 */
void* ping_sender_thread(void* arg) {
    int sockfd = *(int*)arg;
    
    printf("[PING] Thread de envio de pings iniciada\n");
    
    // Inicializa estrutura de saúde dos servidores
    pthread_mutex_lock(&health_mutex);
    for (int i = 0; i < MAX_SERVERS; i++) {
        server_health[i].last_ping_received = time(NULL);
        server_health[i].is_alive = 1;
    }
    pthread_mutex_unlock(&health_mutex);
    
    while (1) {
        sleep(PING_INTERVAL);
        
        packet ping_pkt;
        memset(&ping_pkt, 0, sizeof(ping_pkt));
        ping_pkt.type = PING;
        ping_pkt.seqn = my_id; // Usa seqn para enviar meu ID
        ping_pkt.leader.leader_id = my_id;
        
        // Envia ping para todos os servidores conhecidos
        for (int i = 0; i < num_servers; i++) {
            if (server_list[i].id != my_id) {
                ssize_t sent = sendto(sockfd, &ping_pkt, sizeof(ping_pkt), 0,
                                     (struct sockaddr*)&server_list[i].addr,
                                     sizeof(server_list[i].addr));
                
                if (sent < 0) {
                    // Silencioso - não loga erro para não poluir
                }
            }
        }
    }
    
    return NULL;
}

/**
 * Thread que monitora a saúde dos servidores baseado nos pings
 * @param arg Ponteiro para o socket file descriptor (int*)
 */
void* ping_monitor_thread(void* arg) {
    int sockfd = *(int*)arg;
    
    printf("[PING] Thread de monitoramento iniciada\n");
    
    while (1) {
        sleep(PING_TIMEOUT / 2); // Verifica a cada 3 segundos
        
        time_t now = time(NULL);
        int leader_failed = 0;
        
        pthread_mutex_lock(&health_mutex);
        pthread_mutex_lock(&data_mutex); 
        
        for (int i = 0; i < num_servers; i++) {
            if (server_list[i].id == my_id) {
                continue;
            }

            if (server_health[i].last_ping_received == 0) {
                server_health[i].last_ping_received = now;
                server_health[i].is_alive = 1;
                continue; // Pula a verificação de morte nessa rodada
            }
            
            time_t elapsed = now - server_health[i].last_ping_received;
            
            // --- DETECÇÃO DE FALHA ---
            if (elapsed > PING_TIMEOUT && server_health[i].is_alive) {
                server_health[i].is_alive = 0;
                int failed_id = server_list[i].id;

                printf("[PING] Servidor ID %d morreu (timeout: %ld s). Removendo da lista.\n", 
                       failed_id, elapsed);
                
                // Verifica se foi o líder que morreu ANTES de remover
                if (failed_id == current_leader_id) {
                    leader_failed = 1;
                }

                for (int j = i; j < num_servers - 1; j++) {
                    server_list[j] = server_list[j+1];
                    server_health[j] = server_health[j+1];
                }
                num_servers--;
                i--;
            }
            
            else if (elapsed <= PING_TIMEOUT && !server_health[i].is_alive) {
                server_health[i].is_alive = 1;
                printf("[PING] Servidor ID %d voltou a responder\n", server_list[i].id);
            }
        }
        
        pthread_mutex_unlock(&data_mutex);
        pthread_mutex_unlock(&health_mutex);
        
        if (leader_failed) {
            pthread_mutex_lock(&election_mutex);
            int already_electing = election_in_progress;
            pthread_mutex_unlock(&election_mutex);
            
            if (!already_electing && current_leader_id != my_id) {
                printf("[PING] ==========================================\n");
                printf("[PING] LÍDER (ID %d) FALHOU!\n", current_leader_id);
                printf("[PING] Iniciando eleição imediatamente...\n");
                printf("[PING] ==========================================\n");
                
                start_election(sockfd);
            }
        }
    }
    
    return NULL;
}

/**
 * Processa mensagem PING recebida
 * @param pkt Pacote recebido
 * @param sender_addr Endereço do remetente
 */
void handle_ping(const packet* pkt, const struct sockaddr_in* sender_addr) {
    if (pkt == NULL || sender_addr == NULL) {
        return;
    }
    
    int sender_id = pkt->leader.leader_id;
    
    // Atualiza timestamp do último ping recebido
    pthread_mutex_lock(&health_mutex);
    
    for (int i = 0; i < num_servers; i++) {
        if (server_list[i].id == sender_id) {
            server_health[i].last_ping_received = time(NULL);
            server_health[i].is_alive = 1;
            break;
        }
    }
    
    pthread_mutex_unlock(&health_mutex);
}

/**
 * Inicializa as threads de ping
 * @param sockfd Socket para comunicação
 */
void init_ping_system(int sockfd) {
    static int sock_copy;
    sock_copy = sockfd;
    
    pthread_t sender_tid, monitor_tid;
    
    if (pthread_create(&sender_tid, NULL, ping_sender_thread, &sock_copy) != 0) {
        perror("[PING] Erro ao criar thread de envio");
        return;
    }
    pthread_detach(sender_tid);
    
    if (pthread_create(&monitor_tid, NULL, ping_monitor_thread, &sock_copy) != 0) {
        perror("[PING] Erro ao criar thread de monitoramento");
        return;
    }
    pthread_detach(monitor_tid);
    
    printf("[PING] Sistema de ping inicializado com sucesso\n");
}

/**
 * Reseta os timers de saúde dos servidores (usado após eleição)
 */
void reset_health_timers() {
    pthread_mutex_lock(&health_mutex);
    for (int i = 0; i < MAX_SERVERS; i++) {
        server_health[i].last_ping_received = time(NULL);
        server_health[i].is_alive = 1;
    }
    pthread_mutex_unlock(&health_mutex);
    printf("[DEBUG] Timers de saúde resetados.\n");
}
