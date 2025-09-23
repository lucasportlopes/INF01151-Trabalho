#include "server.h"

Server::Server() : sockfd(-1), n(0)
{
    clilen = sizeof(struct sockaddr_in);
    bzero(&serv_addr, sizeof(serv_addr));
    bzero(&cli_addr, sizeof(cli_addr));
    bzero(buf, 256);
}

Server::~Server()
{
    stop();
}

bool Server::start()
{
    if (!createSocket())
    {
        return false;
    }
    
    if (!bindSocket())
    {
        return false;
    }
    
    printf("passou pelo bind\n");
    return true;
}

void Server::run()
{
    while (1) 
    {
        bzero(buf, 256);
        
        n = recvfrom(sockfd, buf, 256, 0, (struct sockaddr *) &cli_addr, &clilen);
        
        if (n < 0) 
        {
            printf("ERROR on recvfrom");
            continue;
        }
        
        printf("Received a datagram: %s\n", buf);
        
        n = sendto(sockfd, "Got your message\n", 17, 0, (struct sockaddr *) &cli_addr, sizeof(struct sockaddr));
        if (n < 0) 
        {
            printf("ERROR on sendto");
        }
    }
}

void Server::stop()
{
    if (sockfd != -1)
    {
        close(sockfd);
        sockfd = -1;
    }
}

bool Server::createSocket()
{
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) 
    {
        printf("ERROR opening socket");
        return false;
    }
    
    printf("passou pela criacao do socket\n");
    return true;
}

bool Server::bindSocket()
{
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(serv_addr.sin_zero), 8);    
    
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0) 
    {
        printf("ERROR on binding");
        return false;
    }
    
    return true;
}