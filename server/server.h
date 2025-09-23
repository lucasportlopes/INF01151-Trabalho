#ifndef SERVER_H
#define SERVER_H

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>

#define PORT 4000

class Server
{
private:
    int sockfd;
    socklen_t clilen;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    char buf[256];
    int n;
    
public:
    Server();
    ~Server();
    
    bool start();           // Inicializa o servidor (cria socket e faz bind)
    void run();             // Loop principal do servidor
    void stop();            // Para o servidor e fecha socket
    
private:
    bool createSocket();    // Cria o socket UDP
    bool bindSocket();      // Faz o bind do socket
    void handleMessage();   // Processa uma mensagem do cliente
};

#endif // SERVER_H