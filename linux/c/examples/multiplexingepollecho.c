#include <sys/epoll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <fcntl.h>
#define BUFFER_SIZE 1024
char buffer[BUFFER_SIZE] = {0};
unsigned int prevRead = 0;

void print_epoll_event(struct epoll_event *ev)
{
    if (!ev)
    {
        return;
    }

    printf("epoll_event {\n");

    printf("  data.fd: %d\n", ev->data.fd);
    printf("  data.u32: %d\n", ev->data.u32);
    printf("  data.u64: %ld\n", ev->data.u64);

    printf("  events(%d): ", ev->events);

    if (ev->events & EPOLLIN)
        printf("EPOLLIN ");
    if (ev->events & EPOLLPRI)
        printf("EPOLLPRI ");
    if (ev->events & EPOLLOUT)
        printf("EPOLLOUT ");
    if (ev->events & EPOLLERR)
        printf("EPOLLERR ");
    if (ev->events & EPOLLHUP)
        printf("EPOLLHUP ");
    if (ev->events & EPOLLRDHUP)
        printf("EPOLLRDHUP ");
    if (ev->events & EPOLLET)
        printf("EPOLLET ");
    if (ev->events & EPOLLONESHOT)
        printf("EPOLLONESHOT ");
    if (ev->events == 0)
        printf("(none)");

    printf("\n}\n");
}

int main()
{
    int server = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    int serverFlags = fcntl(server, F_GETFL, 0);
    fcntl(server, F_SETFL, serverFlags | O_NONBLOCK);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server, (struct sockaddr *)&addr, sizeof(addr));
    listen(server, 1);

    int epfd = epoll_create1(0);
    if (epfd < 0)
    {
        perror("epoll_create1");
        exit(1);
    }

    printf("epfd: %d\n", epfd);

    struct epoll_event event = {0};

    event.events = EPOLLIN;
    event.data.fd = server;
    int addServer = epoll_ctl(epfd, EPOLL_CTL_ADD, server, &event);
    print_epoll_event(&event);
    if (addServer < 0)
    {
        perror("epoll_ctl:EPOLLIN");
        exit(1);
    }

    while (1)
    {
        printf("epoll_wait\n");
        int wait = epoll_wait(epfd, &event, 1, -1);
        printf("wait: %d\n", wait);
        print_epoll_event(&event);
        printf("*epoll_wait\n");
        if (wait < 0)
        {
            perror("epoll_wait");
            exit(1);
        }
        if (event.events == EPOLLIN)
        {
            if (event.data.fd == server)
            {
                int client = accept(server, NULL, NULL);
                int cleintFlags = fcntl(client, F_GETFL, 0);
                fcntl(client, F_SETFL, cleintFlags | O_NONBLOCK);
                printf("client: %d\n", client);
                if (client < 0)
                {
                    perror("accept");
                    exit(1);
                }
                event.events = EPOLLIN;
                event.data.fd = client;
                int addClient = epoll_ctl(epfd, EPOLL_CTL_ADD, client, &event);
                if (addClient < 0)
                {
                    perror("epoll_ctl:EPOLLIN");
                    exit(1);
                }
                printf("client: %d\n", client);
            }
            else
            {
                int reading = read(event.data.fd, buffer, BUFFER_SIZE);
                printf("reading: %d\n", reading);
                for (size_t i = 0; i < reading; i++)
                {
                    printf("%d,", buffer[i]);
                }
                printf("\n");

                if (reading < 0)
                {
                    perror("reading");
                    exit(1);
                }

                event.events = EPOLLOUT;
                prevRead = reading;
                int modClient = epoll_ctl(epfd, EPOLL_CTL_MOD, event.data.fd, &event);
                if (modClient < 0)
                {
                    perror("epoll_ctl:EPOLLOUT");
                    exit(1);
                }

                if (reading == 0)
                {
                    event.events = EPOLLIN | EPOLLOUT;
                    int removeClient = epoll_ctl(epfd, EPOLL_CTL_DEL, event.data.fd, &event);
                    if (removeClient < 0)
                    {
                        perror("epoll_ctl:EPOLL_CTL_DEL");
                        exit(1);
                    }
                    close(event.data.fd);
                    printf("client(%d): closed\n", event.data.fd);
                }
                else
                {
                    printf("client(%d): %.*s\n", event.data.fd, reading, buffer);
                }
            }
        }

        if (event.events == EPOLLOUT)
        {
            printf("EPOLLOUT\n");
            print_epoll_event(&event);
            int writing = write(event.data.fd, buffer, prevRead);
            printf("writing: %d\n", writing);
            if (writing < 0)
            {
                perror("write");
                exit(1);
            }
            event.events = EPOLLIN;
            event.data.fd = event.data.fd;
            int modClient = epoll_ctl(epfd, EPOLL_CTL_MOD, event.data.fd, &event);
            if (modClient < 0)
            {
                perror("epoll_ctl:EPOLLIN");
                exit(1);
            }
        }
    }

    return 0;
}
