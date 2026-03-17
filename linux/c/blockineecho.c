#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

int main()
{
    int server = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server, (struct sockaddr *)&addr, sizeof(addr));
    listen(server, 10);

    char buffer[4096];

    while (1)
    {
        int client = accept(server, NULL, NULL);

        int n = 0;
        do
        {
            n = read(client, buffer, sizeof(buffer));

            write(client, buffer, n);
        } while (n);

        close(client);
    }
}