#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

#define QUEUE_SIZE 10

int queue[QUEUE_SIZE] = {0};
int count = 0;

void queueAdd(int value)
{
    if (count == QUEUE_SIZE)
    {
        printf("Queue full!\n");
        exit(1);
    }
    queue[count] = value;
    count++;
}

void queueRemove(int index)
{
    if (count == 0)
    {
        return;
    }
    for (int i = index; i < count; i++)
    {
        queue[i] = queue[i + 1];
    }
    count = count - 1;
}

void printQueue()
{
    printf("Queue: [");
    if (count > 0)
    {
        printf("%i", queue[0]);
    }
    for (int i = 1; i < count; i++)
    {
        printf(",%i", queue[i]);
    }
    printf("]\n");
}

int main()
{
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

    while (1)
    {
        int client = accept(server, NULL, NULL);
        if (client > 0)
        {
            int clientFlags = fcntl(client, F_GETFL, 0);
            fcntl(client, F_SETFL, clientFlags | O_NONBLOCK);

            printf("accept: %i \n", client);
            queueAdd(client);
        }
        else if (errno != EAGAIN)
        {
            perror("accept");
        }

        for (int i = 0; i < count; i++)
        {

            int c = queue[i];

            int n = read(c, buffer, sizeof(buffer));
            if (n < 0)
            {
                if (errno != EAGAIN)
                {
                    perror("read");
                    close(c);
                    queueRemove(i);
                    i--;
                    printQueue();
                }
            }
            else if (n == 0)
            {

                close(c);
                queueRemove(i);
                i--;
                printf("Close client: %i\n", c);
                printQueue();
            }
            else
            {

                printQueue();
                printf("Client: %i Read: %i Buffer: %.*s\n", c, n, n, buffer);
                write(c, buffer, n);
            }
        }
    }
}