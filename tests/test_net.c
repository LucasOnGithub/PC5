/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* PC5's HTTPS client (title/pc5_net.c, mbedTLS) against a local server
 * (tests/https_server.py), on the host's sockets.
 *
 *   test_net SPARE_CA.pem CA.pem GOOD_PORT OTHER_CA_PORT
 *
 * The roots are three files as on the console: one missing, one with an
 * unrelated CA, then the one that signed GOOD_PORT's certificate, so the good
 * server is trusted only if every file's roots are kept. */
#include "pc5.h"
#include "pc5_net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int failures, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static int resolve(const char *host, uint32_t *address)
{
    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM }, *found;
    if (getaddrinfo(host, NULL, &hints, &found)) return -1;
    *address = ((struct sockaddr_in *)found->ai_addr)->sin_addr.s_addr;
    freeaddrinfo(found);
    return 0;
}

static int wait_for(int sock, short events, int timeout_ms)
{
    struct pollfd fd = { sock, events, 0 };
    return poll(&fd, 1, timeout_ms) > 0 ? 0 : -1;
}

static int connect_to(uint32_t address, uint16_t port, int timeout_ms)
{
    struct sockaddr_in to = { .sin_family = AF_INET, .sin_port = htons(port), .sin_addr = { address } };
    int sock = socket(AF_INET, SOCK_STREAM, 0), error = 0;
    socklen_t length = sizeof(error);
    if (sock < 0) return -1;
    fcntl(sock, F_SETFL, O_NONBLOCK);
    if (connect(sock, (struct sockaddr *)&to, sizeof(to)) &&
        (errno != EINPROGRESS || wait_for(sock, POLLOUT, timeout_ms) ||
         getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &length) || error)) {
        close(sock);
        return -1;
    }
    return sock;
}

static int send_some(int sock, const void *data, size_t size, int timeout_ms)
{
    for (;;) {
        ssize_t sent = send(sock, data, size, MSG_NOSIGNAL);
        if (sent >= 0) return (int)sent;
        if (errno != EAGAIN || wait_for(sock, POLLOUT, timeout_ms)) return -1;
    }
}

static int recv_some(int sock, void *data, size_t size, int timeout_ms)
{
    for (;;) {
        ssize_t got = recv(sock, data, size, 0);
        if (got >= 0) return (int)got;
        if (errno != EAGAIN || wait_for(sock, POLLIN, timeout_ms)) return -1;
    }
}

static void close_socket(int sock) { close(sock); }

static int random_bytes(void *data, size_t size)
{
    int fd = open("/dev/urandom", O_RDONLY);
    ssize_t got = fd >= 0 ? read(fd, data, size) : -1;
    if (fd >= 0) close(fd);
    return got == (ssize_t)size ? 0 : -1;
}

static void log_line(const char *line) { fprintf(stderr, "  (%s)\n", line); }

int main(int argc, char **argv)
{
    char url[256], host[64], path[64];
    const char *roots[] = { "/nonexistent/CA_LIST.cer", argc > 1 ? argv[1] : "", argc > 2 ? argv[2] : "", NULL };
    Pc5Buf body = { 0 };
    uint16_t port;
    int status;

    if (argc != 5) { fprintf(stderr, "usage: test_net SPARE_CA.pem CA.pem PORT OTHER_PORT\n"); return 2; }
    const Pc5NetOps ops = { resolve, connect_to, send_some, recv_some, close_socket, random_bytes, roots, log_line };
    pc5_net_init(&ops);

    CHECK(!pc5_url_split("https://example.org:8443/a/b?c=d", host, sizeof(host), &port, path, sizeof(path)) &&
          !strcmp(host, "example.org") && port == 8443 && !strcmp(path, "/a/b?c=d"), "split");
    CHECK(!pc5_url_split("https://example.org", host, sizeof(host), &port, path, sizeof(path)) && port == 443 &&
          !strcmp(path, "/"), "split bare");
    CHECK(pc5_url_split("http://example.org/", host, sizeof(host), &port, path, sizeof(path)), "plain http refused");

    snprintf(url, sizeof(url), "https://localhost:%s/small", argv[3]);
    CHECK(!pc5_net_get(NULL, url, &body, 1 << 20, &status) && status == 200 && body.len == 17 &&
          !strcmp((char *)body.data, "{\"hello\":\"world\"}"), "small: %d %zu", status, body.len);
    snprintf(url, sizeof(url), "https://localhost:%s/chunked", argv[3]);
    CHECK(!pc5_net_get(NULL, url, &body, 1 << 20, &status) && status == 200 &&
          !strcmp((char *)body.data, "{\"parts\":[1,2,3]}"), "chunked: %d '%s'", status, (char *)body.data);
    snprintf(url, sizeof(url), "https://localhost:%s/redirect", argv[3]);
    CHECK(!pc5_net_get(NULL, url, &body, 1 << 20, &status) && status == 200 && body.len == 17, "redirect: %d", status);
    snprintf(url, sizeof(url), "https://localhost:%s/agent", argv[3]);
    CHECK(!pc5_net_get(NULL, url, &body, 1 << 20, &status) && strstr((char *)body.data, "PC5/"), "user agent: %s",
          (char *)body.data);
    snprintf(url, sizeof(url), "https://localhost:%s/big", argv[3]);
    CHECK(!pc5_net_get(NULL, url, &body, 2 << 20, &status) && status == 200 && body.len == 1 << 20, "big: %zu", body.len);
    int same = body.len == 1 << 20;
    for (size_t i = 0; same && i < body.len; i++) same = body.data[i] == (uint8_t)((i * 7 + (i >> 9)) & 255);
    CHECK(same, "big body intact");
    CHECK(pc5_net_get(NULL, url, &body, 1000, &status), "a body over the limit is refused");
    snprintf(url, sizeof(url), "https://localhost:%s/missing", argv[3]);
    CHECK(!pc5_net_get(NULL, url, &body, 1 << 20, &status) && status == 404, "404 is an answer: %d", status);
    snprintf(url, sizeof(url), "https://localhost:%s/loop", argv[3]);
    CHECK(pc5_net_get(NULL, url, &body, 1 << 20, &status), "endless redirects stop");
    /* The certificate names localhost, not the address. */
    snprintf(url, sizeof(url), "https://127.0.0.1:%s/small", argv[3]);
    CHECK(pc5_net_get(NULL, url, &body, 1 << 20, &status), "a name that doesn't match is refused");
    /* A server whose certificate no trusted root signed. */
    snprintf(url, sizeof(url), "https://localhost:%s/small", argv[4]);
    CHECK(pc5_net_get(NULL, url, &body, 1 << 20, &status), "an untrusted certificate is refused");
    /* Nobody there. */
    CHECK(pc5_net_get(NULL, "https://localhost:1/small", &body, 1 << 20, &status), "connection refused");
    CHECK(pc5_net_get(NULL, "https://no-such-host.invalid/x", &body, 1 << 20, &status), "no such name");
    pc5_buf_free(&body);
    printf("test_net: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
