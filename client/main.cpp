#include "client.h"

int main(int argc, char *argv[])
{
    if (argc < 2) 
    {
        fprintf(stderr, "usage %s hostname\n", argv[0]);
        exit(0);
    }
    
    Client client;
    
    client.run(argv[1]);
    
    return 0;
}
