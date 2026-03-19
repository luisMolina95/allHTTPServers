#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/select.h>

#define BITS_PER_BYTE 8

void printFDSet(fd_set *set, size_t nb)
{
    unsigned char *bytes = (unsigned char *)set;

    if (nb == 0 || nb > sizeof(fd_set))
    {
        nb = sizeof(fd_set);
    }

    printf("     \u0332");
    for (int fd = 0; fd < BITS_PER_BYTE; fd++)
    {
        printf("%d\u0332", fd % 10);
    }
    printf("\n");

    for (int i = 0; i < nb; i++)
    {
        printf("%02d | ", i * 8);

        for (int bit = 0; bit < 8; bit++)
        {
            int value = (bytes[i] >> bit) & 1;
            printf("%d", value);
        }

        printf("\n");
    }
}

#define BUFFER_SIZE 16384

struct Client
{
    unsigned int read;
    char buffer[BUFFER_SIZE];
};

struct Client clients[FD_SETSIZE] = {0};

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0);
    int server = socket(AF_INET, SOCK_STREAM, 0);
    int serverFlags = fcntl(server, F_GETFL, 0);
    fcntl(server, F_SETFL, serverFlags | O_NONBLOCK);

    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server, (struct sockaddr *)&addr, sizeof(addr));
    listen(server, 1);

    char buffer[4096];

    fd_set readyReadFDSet, allReadFDSet, readyWriteFDSet, allWriteFDSet;
    FD_ZERO(&allReadFDSet);
    FD_ZERO(&allWriteFDSet);
    FD_SET(server, &allReadFDSet);

    while (1)
    {

        printf("allReadFDSet:\n");
        printFDSet(&allReadFDSet, 1);
        printf("allWriteFDSet:\n");
        printFDSet(&allWriteFDSet, 1);

        readyReadFDSet = allReadFDSet;
        readyWriteFDSet = allWriteFDSet;

        printf("Selecting...\n");
        int selecting = select(FD_SETSIZE, &readyReadFDSet, &readyWriteFDSet, NULL, NULL);
        printf("readyReadFDSet:\n");
        printFDSet(&readyReadFDSet, 1);
        printf("readyWriteFDSet:\n");
        printFDSet(&readyWriteFDSet, 1);

        for (int iClient = 0; iClient < FD_SETSIZE; iClient++)
        {

            if (FD_ISSET(iClient, &readyReadFDSet))
            {

                if (iClient == server)
                {

                    printf("Acepting.\n");
                    int client = accept(server, NULL, NULL);
                    printf("Client: %d.\n", client);

                    int clientFlags = fcntl(client, F_GETFL, 0);
                    fcntl(client, F_SETFL, clientFlags | O_NONBLOCK);
                    FD_SET(client, &allReadFDSet);
                }
                else
                {
                    printf("Reading (%d).\n", iClient);
                    int nRead = read(iClient, clients[iClient].buffer, sizeof(clients[iClient].buffer));
                    if (nRead == 0)
                    {
                        close(iClient);
                        FD_CLR(iClient, &allReadFDSet);
                        FD_CLR(iClient, &allWriteFDSet);
                        printf("Close client: %i\n", iClient);
                    }
                    else
                    {

                        clients[iClient].read = nRead;
                        FD_SET(iClient, &allWriteFDSet);
                        printf("Client: %i Read: %i Buffer: %.*s\n", iClient, clients[iClient].read, clients[iClient].read, clients[iClient].buffer);
                    }
                }
            }

            if (FD_ISSET(iClient, &readyWriteFDSet))
            {
                write(iClient, clients[iClient].buffer, clients[iClient].read);
                FD_CLR(iClient, &allWriteFDSet);
            }
        }
    }
}