#ifndef CLIENT_H
#define CLIENT_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define PORT 4000

class Client
{
private:
    int sockfd;
    int n;
    unsigned int length;
    struct sockaddr_in serv_addr;
    struct sockaddr_in from;
    struct hostent *server;
    char buffer[256];
    
public:
    Client();
    ~Client();
    
    bool connect(const char* hostname);     // Conecta ao servidor
    bool sendMessage();                     // Envia mensagem
    bool receiveMessage();                  // Recebe resposta
    void disconnect();                      // Desconecta do servidor
    void run(const char* hostname);         // Executa o fluxo completo do cliente
    
private:
    bool createSocket();                    // Cria o socket UDP
    bool setupServer(const char* hostname); // Configura servidor
};

#endif // CLIENT_H