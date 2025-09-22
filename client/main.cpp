#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080

int main()
{
    int sockfd;
    struct sockaddr_in servaddr;
    char buffer[1024];

    // Criar socket UDP
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Erro ao criar socket");
        return 1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = INADDR_ANY; // localhost

    std::string msg = "Olá servidor!";
    sendto(sockfd, msg.c_str(), msg.size(), 0, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    std::cout << "Mensagem enviada: " << msg << std::endl;

    socklen_t len = sizeof(servaddr);
    int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&servaddr, &len);
    buffer[n] = '\0';
    std::cout << "Resposta do servidor: " << buffer << std::endl;

    close(sockfd);
    return 0;
}
