#ifndef REPLICATION_H
#define REPLICATION_H

#include "server.h"

// Funções para eleição de líder (Algoritmo do Valentão)
void start_election(int sockfd);
void handle_election_message(int sockfd, const packet* pkt, const struct sockaddr_in* sender_addr);
void handle_answer_message(const packet* pkt);
void handle_coordinator_message(const packet* pkt);
void promote_to_primary(int sockfd);

// Funções para replicação de estado
void propagate_state_to_backups(int sockfd);
void apply_state_update(const struct replication_msg* rep);
void handle_replication_update(const packet* pkt, int sockfd, const struct sockaddr_in* sender_addr);

// Funções para sistema de ping/heartbeat
void* ping_sender_thread(void* arg);
void* ping_monitor_thread(void* arg);
void handle_ping(const packet* pkt, const struct sockaddr_in* sender_addr);
void init_ping_system(int sockfd);

#endif // REPLICATION_H
