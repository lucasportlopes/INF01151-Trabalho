#ifndef INTERFACE_H
#define INTERFACE_H

#include <stdio.h>
#include <time.h>

void log_connection(char* server_ip);

void log_timeout();

void log_request(char* server_ip, char* dest_ip, uint32_t req_id, uint32_t value, uint32_t new_balance);

#endif // INTERFACE_H
