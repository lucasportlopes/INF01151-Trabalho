#include "server.h"

int main(int argc, char *argv[])
{
    Server server;
    
    if (!server.start())
    {
        printf("Falha ao inicializar o servidor!\n");
        return 1;
    }
    
    server.run();
    
    return 0;
}
