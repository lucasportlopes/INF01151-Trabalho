#include "client.h"

Client::Client() : sockfd(-1), n(0), length(0)
{
    bzero(&serv_addr, sizeof(serv_addr));
    bzero(&from, sizeof(from));
    bzero(buffer, 256);
    server = NULL;
}

Client::~Client()
{
    disconnect();
}

bool Client::connect(const char* hostname)
{
    if (!setupServer(hostname))
    {
        return false;
    }
    
    if (!createSocket())
    {
        return false;
    }
    
    return true;
}

bool Client::sendMessage()
{
    printf("Enter the message: ");
    bzero(buffer, 256);
    if (fgets(buffer, 256, stdin) == NULL) {
        printf("ERROR reading input");
        return false;
    }

    n = sendto(sockfd, buffer, strlen(buffer), 0, (const struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
    if (n < 0) 
    {
        printf("ERROR sendto");
        return false;
    }
    
    return true;
}

bool Client::receiveMessage()
{
    length = sizeof(struct sockaddr_in);
    n = recvfrom(sockfd, buffer, 256, 0, (struct sockaddr *) &from, &length);
    if (n < 0)
    {
        printf("ERROR recvfrom");
        return false;
    }

    printf("Got an ack: %s\n", buffer);
    return true;
}

void Client::disconnect()
{
    if (sockfd != -1)
    {
        close(sockfd);
        sockfd = -1;
    }
}

void Client::run(const char* hostname)
{
    if (!connect(hostname))
    {
        printf("Falha ao conectar ao servidor!\n");
        exit(1);
    }
    
    if (!sendMessage())
    {
        printf("Falha ao enviar mensagem!\n");
        exit(1);
    }
    
    if (!receiveMessage())
    {
        printf("Falha ao receber resposta!\n");
        exit(1);
    }
}

bool Client::setupServer(const char* hostname)
{
    server = gethostbyname(hostname);

    if (server == NULL) 
    {
        fprintf(stderr,"ERROR, no such host\n");
        return false;
    }
    
    serv_addr.sin_family = AF_INET;     
    serv_addr.sin_port = htons(PORT);    
    serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
    bzero(&(serv_addr.sin_zero), 8);
    
    return true;
}

bool Client::createSocket()
{
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        printf("ERROR opening socket");
        return false;
    }
    
    return true;
}
