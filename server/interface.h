#ifndef INTERFACE_H
#define INTERFACE_H

#include <stdio.h>
#include <time.h>
#include <stdint.h>

void log_history(int num_transactions, int total_transferred, int total_balance);

void log_discovery(char *client);

void log_request(char *client_ip, char *dest_ip, uint32_t req_id, uint32_t value, int num_transactions, int total_transferred, int total_balance);

void log_duplicate_request(char *client_ip, char *dest_ip, uint32_t req_id, uint32_t value, int num_transactions, int total_transferred, int total_balance);

#endif // INTERFACE_H
