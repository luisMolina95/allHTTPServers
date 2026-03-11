#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 4096

// Read exactly n bytes from socket
ssize_t read_n_bytes(int fd, char *buffer, size_t n) {
    size_t total = 0;
    while (total < n) {
        ssize_t r = read(fd, buffer + total, n - total);
        if (r <= 0) return r; // error or EOF
        total += r;
    }
    return total;
}

void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }
    buffer[bytes_read] = '\0';

    // GET /data returns JSON
    if (strncmp(buffer, "GET /data", 9) == 0) {
        const char *response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: 27\r\n"
            "\r\n"
            "{\"message\":\"Hello JSON!\"}";
        write(client_fd, response, strlen(response));
    }
    // POST /upload streams file
    else if (strncmp(buffer, "POST /upload", 12) == 0) {
        // Find Content-Length header
        char *cl = strstr(buffer, "Content-Length:");
        if (!cl) {
            const char *err =
                "HTTP/1.1 411 Length Required\r\n"
                "Content-Length: 0\r\n\r\n";
            write(client_fd, err, strlen(err));
            close(client_fd);
            return;
        }

        size_t content_length = atoi(cl + 15); // skip "Content-Length: "
        char *body = strstr(buffer, "\r\n\r\n");
        size_t body_bytes = 0;
        if (body) {
            body += 4; // skip \r\n\r\n
            body_bytes = bytes_read - (body - buffer);
        }

        FILE *fp = fopen("uploaded_file.bin", "wb");
        if (!fp) {
            const char *err =
                "HTTP/1.1 500 Internal Server Error\r\n"
                "Content-Length: 21\r\n\r\n"
                "Failed to save file";
            write(client_fd, err, strlen(err));
            close(client_fd);
            return;
        }

        // Write the initial body chunk if present
        if (body_bytes > 0) {
            fwrite(body, 1, body_bytes, fp);
        }

        size_t remaining = content_length - body_bytes;
        while (remaining > 0) {
            ssize_t r = read(client_fd, buffer, remaining > BUFFER_SIZE ? BUFFER_SIZE : remaining);
            if (r <= 0) break;
            fwrite(buffer, 1, r, fp);
            remaining -= r;
        }

        fclose(fp);

        const char *ok =
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 16\r\n"
            "\r\n"
            "File uploaded OK";
        write(client_fd, ok, strlen(ok));
    }
    else {
        const char *not_found =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: 9\r\n"
            "\r\n"
            "Not Found";
        write(client_fd, not_found, strlen(not_found));
    }

    close(client_fd);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        exit(1);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&addr, &addr_len);
        if (client_fd < 0) { perror("accept"); continue; }
        handle_client(client_fd);
    }

    close(server_fd);
    return 0;
}