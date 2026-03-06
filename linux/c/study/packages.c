#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <ctype.h>

// Print TCP payload as hex + ASCII
void print_payload(const unsigned char *data, int len)
{
    for (int i = 0; i < len; i++)
    {
        printf("%02x ", data[i]);
        if ((i + 1) % 16 == 0)
            printf("  ");
        if ((i + 1) % 16 == 0 || i == len - 1)
        {
            int j = i - ((i) % 16);
            printf(" | ");
            for (; j <= i; j++)
                printf("%c", isprint(data[j]) ? data[j] : '.');
            printf("\n");
        }
    }
}

void print_flags(struct tcphdr *tcp_hdr)
{
    printf("%c%c%c%c%c%c",
           (tcp_hdr->urg ? 'U' : '-'),
           (tcp_hdr->ack ? 'A' : '-'),
           (tcp_hdr->psh ? 'P' : '-'),
           (tcp_hdr->rst ? 'R' : '-'),
           (tcp_hdr->syn ? 'S' : '-'),
           (tcp_hdr->fin ? 'F' : '-'));
}

int main()
{
    const char *hostname = "example.com";

    // Resolve hostname
    struct hostent *he = gethostbyname(hostname);
    if (!he)
    {
        perror("gethostbyname");
        return 1;
    }
    struct in_addr target_ip = *((struct in_addr *)he->h_addr);

    int raw_sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (raw_sock < 0)
    {
        perror("raw socket");
        return 1;
    }

    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock < 0)
    {
        perror("tcp socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(80);
    server_addr.sin_addr = target_ip;

    if (connect(tcp_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect");
        return 1;
    }

    struct sockaddr_in local_addr;
    socklen_t len = sizeof(local_addr);
    getsockname(tcp_sock, (struct sockaddr *)&local_addr, &len);
    uint16_t local_port = ntohs(local_addr.sin_port);

    // Send HTTP GET
    const char *request = "GET / HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n";
    send(tcp_sock, request, strlen(request), 0);

    printf("\nCapturing packets for this GET request:\n\n");

    unsigned char buffer[65536];
    int captured = 0;
    while (captured < 10)
    {
        int size = recv(raw_sock, buffer, sizeof(buffer), 0);
        if (size < 0)
            continue;

        struct ip *ip_hdr = (struct ip *)buffer;
        int ip_header_len = ip_hdr->ip_hl * 4;

        if (ip_hdr->ip_p == IPPROTO_TCP)
        {
            struct tcphdr *tcp_hdr = (struct tcphdr *)(buffer + ip_header_len);
            uint16_t src_port = ntohs(tcp_hdr->source);
            uint16_t dst_port = ntohs(tcp_hdr->dest);

            if ((src_port == 80 && dst_port == local_port) || (dst_port == 80 && src_port == local_port))
            {

                char *direction = (src_port == local_port) ? "SENT" : "RECEIVED";
                printf("src_port:%hu , local_port:%hu\n", src_port, local_port);

                printf("Packet %d [%s]:\n", captured + 1, direction);
                char src_ip[INET_ADDRSTRLEN];
                char dst_ip[INET_ADDRSTRLEN];

                strcpy(src_ip, inet_ntoa(ip_hdr->ip_src));
                strcpy(dst_ip, inet_ntoa(ip_hdr->ip_dst));
                printf("IPv4 Header: Src=%s Dst=%s TotalLen=%d TTL=%d Protocol=%d\n",
                       src_ip,
                       dst_ip,
                       ntohs(ip_hdr->ip_len),
                       ip_hdr->ip_ttl,
                       ip_hdr->ip_p);

                printf("TCP Header: SrcPort=%d DstPort=%d Seq=%u Ack=%u Flags=",
                       src_port, dst_port, ntohl(tcp_hdr->seq), ntohl(tcp_hdr->ack_seq));
                print_flags(tcp_hdr);
                printf("\n");

                int tcp_payload_len = ntohs(ip_hdr->ip_len) - ip_header_len - tcp_hdr->doff * 4;
                printf("TCP Payload (%d bytes):\n", tcp_payload_len);
                if (tcp_payload_len > 0)
                    print_payload(buffer + ip_header_len + tcp_hdr->doff * 4, tcp_payload_len);
                else
                    printf("(No payload)\n");

                printf("-----------------------------\n");
                captured++;
            }
        }
    }

    close(tcp_sock);
    close(raw_sock);
    return 0;
}
