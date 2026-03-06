#include <sys/socket.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>

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
    if (sock < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    return 0;
}