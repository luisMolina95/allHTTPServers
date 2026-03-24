#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_EVENTS 64
#define MAX_CONNECTIONS 1024
#define REQ_BUF_SIZE 8192
#define RESP_BUF_SIZE 512
#define FILE_BUF_SIZE 32768

typedef struct conn {
    int fd;
    int slot;
    FILE *fp;

    int stage; /* 0 = reading request, 1 = writing response */

    char req[REQ_BUF_SIZE];
    size_t req_len;

    char header[RESP_BUF_SIZE];
    size_t header_len;
    size_t header_off;

    char filebuf[FILE_BUF_SIZE];
    size_t file_len;
    size_t file_off;
} conn_t;

static conn_t conns[MAX_CONNECTIONS];
static int free_slots[MAX_CONNECTIONS];
static int free_top = 0;

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

static void pool_init(void) {
    for (int i = MAX_CONNECTIONS - 1; i >= 0; --i) {
        free_slots[free_top++] = i;
    }
}

static conn_t *conn_alloc(void) {
    if (free_top <= 0) return NULL;

    int slot = free_slots[--free_top];
    conn_t *c = &conns[slot];
    memset(c, 0, sizeof(*c));
    c->slot = slot;
    return c;
}

static void conn_release(conn_t *c) {
    if (!c) return;
    free_slots[free_top++] = c->slot;
}

static void ep_mod(int ep, int fd, uint64_t token, uint32_t events) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.u64 = token;
    epoll_ctl(ep, EPOLL_CTL_MOD, fd, &ev);
}

static void ep_add(int ep, int fd, uint64_t token, uint32_t events) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.u64 = token;
    epoll_ctl(ep, EPOLL_CTL_ADD, fd, &ev);
}

static void close_conn(int ep, conn_t *c) {
    if (!c) return;

    epoll_ctl(ep, EPOLL_CTL_DEL, c->fd, NULL);

    if (c->fp) {
        fclose(c->fp);
        c->fp = NULL;
    }

    close(c->fd);
    c->fd = -1;
    conn_release(c);
}

static void make_404(conn_t *c) {
    if (c->fp) {
        fclose(c->fp);
        c->fp = NULL;
    }

    c->header_len = (size_t)snprintf(
        c->header,
        sizeof(c->header),
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Length: 3\r\n"
        "Connection: close\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "404"
    );

    c->header_off = 0;
    c->file_len = 0;
    c->file_off = 0;
    c->stage = 1;
}

static void make_200(conn_t *c, const char *fullpath) {
    struct stat st;

    if (c->fp) {
        fclose(c->fp);
        c->fp = NULL;
    }

    if (stat(fullpath, &st) < 0) {
        make_404(c);
        return;
    }

    c->fp = fopen(fullpath, "rb");
    if (!c->fp) {
        make_404(c);
        return;
    }

    c->header_len = (size_t)snprintf(
        c->header,
        sizeof(c->header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n"
        "Content-Type: application/octet-stream\r\n"
        "\r\n",
        (long long)st.st_size
    );

    c->header_off = 0;
    c->file_len = 0;
    c->file_off = 0;
    c->stage = 1;
}

static void handle_read(int ep, conn_t *c) {
    for (;;) {
        if (c->req_len >= sizeof(c->req) - 1) {
            make_404(c);
            ep_mod(ep, c->fd, (uint64_t)(uintptr_t)c, EPOLLOUT);
            return;
        }

        ssize_t r = read(c->fd, c->req + c->req_len, (sizeof(c->req) - 1) - c->req_len);

        if (r > 0) {
            c->req_len += (size_t)r;
            c->req[c->req_len] = '\0';

            if (strstr(c->req, "\r\n\r\n") != NULL) {
                char path[512];
                char fullpath[1024];

                if (sscanf(c->req, "GET %511s", path) != 1) {
                    make_404(c);
                    ep_mod(ep, c->fd, (uint64_t)(uintptr_t)c, EPOLLOUT);
                    return;
                }

                char *q = strchr(path, '?');
                if (q) *q = '\0';

                if (strcmp(path, "/") == 0) {
                    strcpy(path, "/index.html");
                }

                if (strstr(path, "..") != NULL) {
                    make_404(c);
                    ep_mod(ep, c->fd, (uint64_t)(uintptr_t)c, EPOLLOUT);
                    return;
                }

                int n = snprintf(fullpath, sizeof(fullpath), "./files%s", path);
                if (n < 0 || n >= (int)sizeof(fullpath)) {
                    make_404(c);
                    ep_mod(ep, c->fd, (uint64_t)(uintptr_t)c, EPOLLOUT);
                    return;
                }

                make_200(c, fullpath);
                ep_mod(ep, c->fd, (uint64_t)(uintptr_t)c, EPOLLOUT);
                return;
            }

            continue;
        }

        if (r == 0) {
            close_conn(ep, c);
            return;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }

        close_conn(ep, c);
        return;
    }
}

static void handle_write(int ep, conn_t *c) {
    while (c->header_off < c->header_len) {
        ssize_t w = write(c->fd, c->header + c->header_off, c->header_len - c->header_off);

        if (w > 0) {
            c->header_off += (size_t)w;
            continue;
        }

        if (w == 0) {
            close_conn(ep, c);
            return;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }

        close_conn(ep, c);
        return;
    }

    if (!c->fp) {
        close_conn(ep, c);
        return;
    }

    for (;;) {
        if (c->file_off == c->file_len) {
            c->file_len = fread(c->filebuf, 1, sizeof(c->filebuf), c->fp);
            c->file_off = 0;

            if (c->file_len == 0) {
                close_conn(ep, c);
                return;
            }
        }

        while (c->file_off < c->file_len) {
            ssize_t w = write(c->fd, c->filebuf + c->file_off, c->file_len - c->file_off);

            if (w > 0) {
                c->file_off += (size_t)w;
                continue;
            }

            if (w == 0) {
                close_conn(ep, c);
                return;
            }

            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }

            close_conn(ep, c);
            return;
        }

        c->file_off = 0;
        c->file_len = 0;
    }
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    pool_init();

    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) return 1;

    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    set_nonblocking(server);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;
    if (listen(server, 128) < 0) return 1;

    int ep = epoll_create1(0);
    if (ep < 0) return 1;

    ep_add(ep, server, 0, EPOLLIN);

    struct epoll_event events[MAX_EVENTS];

    for (;;) {
        int n = epoll_wait(ep, events, MAX_EVENTS, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < n; ++i) {
            if (events[i].data.u64 == 0) {
                for (;;) {
                    int cfd = accept(server, NULL, NULL);
                    if (cfd < 0) {
                        if (errno == EINTR) continue;
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        break;
                    }

                    set_nonblocking(cfd);

                    conn_t *c = conn_alloc();
                    if (!c) {
                        close(cfd);
                        continue;
                    }

                    c->fd = cfd;
                    c->stage = 0;
                    c->req_len = 0;

                    ep_add(ep, cfd, (uint64_t)(uintptr_t)c, EPOLLIN);
                }
                continue;
            }

            conn_t *c = (conn_t *)(uintptr_t)events[i].data.u64;

            if (events[i].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)) {
                close_conn(ep, c);
                continue;
            }

            if (c->stage == 0) {
                handle_read(ep, c);
            } else {
                handle_write(ep, c);
            }
        }
    }

    close(server);
    close(ep);
    return 0;
}