#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

int main()
{
    int sock;
    struct sockaddr_in server_addr;
    const char *msg = "Hello, server!";
    char buffer[1024] = {0};

    // 1. Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    printf("[LOG] Socket created. FD = %d\n", sock);

    // 2. Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
    {
        perror("[ERROR] Invalid IP address");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("[LOG] Server address set to 127.0.0.1:8080\n");

    // 3. Connect to server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("[ERROR] connect failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("[LOG] Connected to server!\n");

    // 4. Send message
    ssize_t sent_bytes = send(sock, msg, strlen(msg), 0);
    if (sent_bytes < 0)
    {
        perror("[ERROR] send failed");
    }
    else
    {
        printf("[LOG] Sent %zd bytes: %s\n", sent_bytes, msg);
    }

    // 5. Receive message
    ssize_t recv_bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (recv_bytes < 0)
    {
        perror("[ERROR] recv failed");
    }
    else if (recv_bytes == 0)
    {
        printf("[LOG] Server closed the connection.\n");
    }
    else
    {
        buffer[recv_bytes] = '\0';
        printf("[LOG] Received %zd bytes: %s\n", recv_bytes, buffer);
    }

    // 6. Close socket
    if (close(sock) < 0)
    {
        perror("[ERROR] Failed to close socket");
    }
    else
    {
        printf("[LOG] Socket closed successfully.\n");
    }

    return 0;
}