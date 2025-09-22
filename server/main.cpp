#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080

int main()
{
    int sockfd;
    char buffer[1024];
    struct sockaddr_in servaddr, cliaddr;

    // Criar socket UDP
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Erro ao criar socket");
        return 1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    // Associar socket ao endereço/porta
    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("Erro no bind");
        close(sockfd);
        return 1;
    }

    std::cout << "Servidor UDP rodando na porta " << PORT << "...\n";

    socklen_t len = sizeof(cliaddr);
    int n;
    while (true)
    {
        n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&cliaddr, &len);
        buffer[n] = '\0';
        std::cout << "Cliente disse: " << buffer << std::endl;

        std::string resposta = "Mensagem recebida!";
        sendto(sockfd, resposta.c_str(), resposta.size(), 0, (struct sockaddr *)&cliaddr, len);
    }

    close(sockfd);
    return 0;
}
