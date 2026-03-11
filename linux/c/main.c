#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/un.h>
#include <stddef.h>
#include <string.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>

char buffer[1024] = {0};

void print_sockaddr_in(struct sockaddr_in *sa)
{
    char ip_buffer[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(sa->sin_addr), ip_buffer, INET_ADDRSTRLEN);
    unsigned short port = ntohs(sa->sin_port);
    printf("Family: %i, Address: %s, Port: %u\n", sa->sin_family, ip_buffer, port);
}
int main()
{
    // 1. Create Socket
    // socket fn runs the syscalls to create a socket
    // socket(domain, type, protocol)
    // domain(AF_INET)      : It defines addressing. AF_INET, for example, means we are going to use IPv4 addresses (xx.xx.xx.xx)
    //                        (It could also be IPv6 addresses, or a .sock to communicate between executables, etc.)
    // type(SOCK_STREAM)    : Now that they know where they are, type defines how the communication flows. SOCK_STREAM means that it will be a stream, meaning both sockets will talk and listen.
    //                        (It can also be SOCK_DGRAM, when only one talks and the other just listens, or SOCK_RAW if you need to handle everything.)
    // protocol(IPPROTO_TCP): Now that both sockets are able to talk and listen to each other, we define the language they actually are talking. This defines what is talked and what is expected to be listened.
    //                        (In TCP, for example, this means: first we do the handshake, then we send the packets with the payload, and we'll send all in order and if somethig fails we'll retry it, and then when done we close the connection.)
    // socket fn returns the File Descriptor (an integer number). Despite the name, FD refers to the ID of a resource where data is being read and written (traveling)
    // (not necessarily always a file; it could be just in RAM, for example). In this case, the FD is the ID of the socket.
    // If it fails, it returns -1 and sets errno
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    printf("FD - Socket ID: %i\n", sock);
    if (sock < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Bind Socket
    // Now we need to assign an address to our socket (binding).
    // The bind function runs the syscalls to bind a socket to an address.
    // bind(socket, sockaddr, socklen)
    // socket (sock)          : It's the FD, the ID of the socket.
    // sockaddr (name)        : The address depends on whether you are running a server or a client.
    //                          For a client, it is the IP of the server you want to connect to.
    //                          For a server, it is the IP of your network device.
    //                          (The computer can have multiple network devices; for example, your laptop can have both an Ethernet adapter and a Wi-Fi adapter.
    //                          Here you specify which one, or all of them with 0.0.0.0, or just your PC with 127.0.0.x.)
    //                          For servers, we use .sin_addr = {.s_addr = ...}, for clients just .sin_addr = ... .
    //                          There are specific sockaddr structs depending on the type: sockaddr_in for IPv4, sockaddr_in6 for IPv6, or sockaddr_un for Unix sockets.
    //                          All of them "inherit" from struct sockaddr. All of them are 16 bytes. Don't get distracted by this; they are like overloading the bind function
    //                          with the __SOCKADDR_COMMON macro and (struct sockaddr *) casting. I don't fully get it, but it does not matter right now.
    //                          The port is a number from 0 to 65,535 that distinguishes the socket on a machine. A FD is more like a kernel opening a tunel for I/O data, then we use this tunel for whatever, and its id per process).
    //                          , the port is the actual ID of the socket you are going to use online, not just internally in the computer like the FD, we use this only to listen incoming connections, not reading not writing.
    //                          It's one-to-one: one socket, one port.
    // socklen (sizeof(name)) : It's C, so we need to tell it the size of the structure so it knows when to stop.

    struct sockaddr_in name = {
        .sin_family = AF_INET,
        .sin_port = htons(8080),
        .sin_addr = {.s_addr = htonl(INADDR_ANY)}};

    int binding = bind(sock, (struct sockaddr *)&name, sizeof(name));
    printf("binding: %i\n", binding);
    if (binding < 0)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    // 3. Listen Socket
    // Now we need to activate the socket and set the queue limit.
    // The listen function runs the syscalls to reserve space for the queue and sets the socket to active.
    // This automatically sends the handshake to all on the queue limit.
    // listen(sock, queue)
    // sock (sock): FD of the socket
    // queue (1)  : queue limit + 1, max connection is 4096 (SOMAXCONN 4096), Max n fully-handshaked connections
    //              This is kind of wierd, clients do not receve a connection refused when overflowing, It jsut silently ignores them,
    //              and the clients just keep on retying to the infinate, if you dont't set a timeout on the client
    //              The only thing i noticed Is that the overflown connections dont get an 3way handshake (Never actually carries the data. Only queues handshakes)
    const int listening = listen(sock, 1);
    printf("listening: %i\n", listening);
    if (listening < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    for (;;)
    {
        printf("START:1 \n");
        struct sockaddr_in clientName = {0};
        socklen_t size = sizeof clientName;
        // 3. Accept Connection
        // Accept fn pulls thge first connection from the queue of 3 handshaked clients
        // accept(socket, &socketAddress, &size)
        // sock (sock): the FD of the socket
        // socketAddress(clientName): The structure where accept is going to save the data of the client
        // size(size): The size of the client address, it's a pointer because the size can change if smaller
        // Returns a brand new FD separate from the listening socket to handle the connection betwwen the client and the server, we use this one for read and write
        int clientFD = accept(sock, (struct sockaddr *)&clientName, &size);
        printf("clientFD: %i\n", clientFD);

        print_sockaddr_in(&clientName);
        if (clientFD < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        void *readBuff = (void *)buffer;
        
        for (;;)
        {
            printf("START:1:2 \n");
            int reading = read(clientFD, readBuff, 10);
            printf("reading: %i\n", reading);
            printf("buffer: %s\n", buffer);

            if (reading < 0)
            {
                perror("read");
                exit(EXIT_FAILURE);
            }
            if (strstr(buffer, "\r\n\r\n"))
            {
                break;
            }
            readBuff = readBuff + reading;
            printf("END:1:1 \n");
        }
        close(clientFD);
        close(sock);
        printf("END:1 \n");
        break;
    }

    printf("DONE!\n");
    return 0;
}