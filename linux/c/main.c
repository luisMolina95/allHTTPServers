#include <netinet/in.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#define SINGLE_EVENT 1
#define INFTIM -1
#define PORT 8080

#define BUFFER_SIZE 16384
char buffer[BUFFER_SIZE] = {0};
int r = 0;
char path[1024] = {0};
char fullPath[1032] = {0};
char header[512] = {0};
FILE *file = {0};

void print_bits(uint32_t mask)
{
    printf("0x%08x | ", mask);
    for (int i = 31; i >= 0; i--)
    {
        printf("%d", (mask >> i) & 1);
        if (i % 8 == 0)
            printf(" "); // optional grouping
    }
    printf("\n");
}

int main()
{
    signal(SIGPIPE, SIG_IGN);
    int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set Server Socket to Resusable
    int opt = 1;
    if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Set Server Socket FD to Non-Blocking
    int serverFlags = fcntl(server, F_GETFL, 0);
    if (serverFlags < 0)
    {
        perror("fcntl:F_GETFL");
        exit(EXIT_FAILURE);
    }
    if (fcntl(server, F_SETFL, serverFlags | O_NONBLOCK) < 0)
    {
        perror("fcntl:F_SETFL");
        exit(EXIT_FAILURE);
    }

    // Bind Socket to all Network Devices Port 8080
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = INADDR_ANY;
    bind(server, (struct sockaddr *)&address, sizeof(address));

    // Listen to incoming connections
    if (listen(server, 1))
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Create Event Poll
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0)
    {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    // ADD Server Socket FD to epoll Set
    struct epoll_event event = {0};
    event.events = EPOLLIN;
    event.data.fd = server;
    print_bits(event.events);
    int addServer = epoll_ctl(epfd, EPOLL_CTL_ADD, server, &event);
    if (addServer < 0)
    {
        perror("epoll_ctl:EPOLL_CTL_ADD:EPOLLIN");
        exit(EXIT_FAILURE);
    }

    for (;;)
    {
        // Poll for ready events (Blocks)
        // n: Number of ready events
        printf("waiting...\n");
        int n = epoll_wait(epfd, &event, SINGLE_EVENT, INFTIM);
        print_bits(event.events);
        printf("n: %d\n", n);
        if (n < 0)
        {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        if (event.events & EPOLLIN)
        {
            if (event.data.fd == server)
            {
                // Acept new connection
                int client = accept(server, NULL, NULL);
                if (client < 0)
                {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }

                // Set Client FD to Non-Blocking
                int clientFlags = fcntl(client, F_GETFL, 0);
                fcntl(client, F_SETFL, clientFlags | O_NONBLOCK);

                // Add Client FD to the set
                // Only notify when stage changes to readable
                event.events = EPOLLIN | EPOLLET;
                event.data.fd = client;
                int addClient = epoll_ctl(epfd, EPOLL_CTL_ADD, client, &event);
                if (addClient < 0)
                {
                    perror("epoll_ctl:EPOLL_CTL_ADD:EPOLLIN | EPOLLET");
                    exit(EXIT_FAILURE);
                }

                printf("client connected(%d)\n", client);
            }
            else
            {
                // Read request
                printf("read %d\n", event.data.fd);
                r = read(event.data.fd, buffer, BUFFER_SIZE);
                if (r < 0)
                {
                    perror("read");
                    printf("errno(%d)\n", errno);
                    exit(EXIT_FAILURE);
                }
                printf("buffer(%d): %.*s\n", r, r, buffer);

                // Parse Path from HTTP request
                sscanf(buffer, "%*s %1023s", path);
                printf("path:\n%s\n", path);

                // Build full path
                snprintf(fullPath, sizeof(fullPath), "./files%s", path);
                printf("fullPath:%s\n", fullPath);

                // Set client to write event
                event.events = EPOLLOUT;

                int modClient = epoll_ctl(epfd, EPOLL_CTL_MOD, event.data.fd, &event);

                if (modClient < 0)
                {
                    perror("epoll_ctl:EPOLL_CTL_MOD:EPOLLOUT | EPOLLET");
                    exit(EXIT_FAILURE);
                }
            }
        }

        if (event.events & EPOLLOUT)
        {

            if (file == NULL)
            {

                file = fopen(fullPath, "rb");
                printf("file: %p\n", file);
                if (file == NULL)
                {

                    if (errno == ENOENT)
                    {

                        write(event.data.fd, "HTTP/1.1 404 Not Found\r\n", 25);
                    }
                    else
                    {
                        perror("fopen");
                    }

                    if (epoll_ctl(epfd, EPOLL_CTL_DEL, event.data.fd, NULL) < 0)
                    {
                        perror("epoll_ctl:EPOLL_CTL_DEL");
                        exit(EXIT_FAILURE);
                    }
                    shutdown(event.data.fd, SHUT_RDWR);
                    close(event.data.fd);
                    printf("client closed(%d)\n", event.data.fd);
                    continue;
                }

                // Get file size
                fseek(file, 0, SEEK_END);
                unsigned long fileSize = ftell(file);
                printf("fileSize(%ld)\n", fileSize);
                rewind(file);

                // Write minimal HTTP header
                snprintf(header, sizeof(header),
                         "HTTP/1.1 200 OK\r\n"
                         "Content-Length: %lu\r\n"
                         "\r\n",
                         fileSize);

                if (write(event.data.fd, header, strlen(header)) < 0)
                {
                    perror("write");
                    exit(EXIT_FAILURE);
                }
            }

            int fileReading = 0;
            int writing = 0;
            do
            {

                fileReading = fread(buffer, 1, BUFFER_SIZE - 1, file);
                if (fileReading < 0)
                {
                    perror("fread");
                    exit(EXIT_FAILURE);
                }

                writing = write(event.data.fd, buffer, fileReading);
                if (writing != -1 && writing != fileReading)
                {
                    printf("read(%d), wrote(%d)...\n", fileReading, writing);
                }

                if (writing == 0)
                {
                    if (epoll_ctl(epfd, EPOLL_CTL_DEL, event.data.fd, NULL) < 0)
                    {

                        perror("epoll_ctl:EPOLL_CTL_DEL");
                        printf("errno(%d)\n", errno);
                        exit(EXIT_FAILURE);
                    }
                    shutdown(event.data.fd, SHUT_RDWR);
                    close(event.data.fd);
                    fclose(file);
                    file = NULL;
                    printf("client closed(%d)\n", event.data.fd);
                    break;
                }

                if (writing < 0)
                {
                    if (errno == ECONNRESET || errno == EBADF || errno == EPIPE)
                    {
                        if (epoll_ctl(epfd, EPOLL_CTL_DEL, event.data.fd, NULL) < 0)
                        {
                            if (errno != EBADF)
                            {
                                perror("epoll_ctl:EPOLL_CTL_DEL");
                                printf("errno(%d)\n", errno);
                                exit(EXIT_FAILURE);
                            }
                        }
                        shutdown(event.data.fd, SHUT_RDWR);
                        close(event.data.fd);
                        fclose(file);
                        file = NULL;
                        printf("client closed(%d)\n", event.data.fd);
                        break;
                    }
                    else if (errno == EAGAIN)
                    {
                        break;
                    }
                    else
                    {
                        perror("write");
                        printf("errno(%d)\n", errno);
                        exit(EXIT_FAILURE);
                    }
                }
            } while (fileReading > 0);
        }
    }

    return 0;
}