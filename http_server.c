#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <netinet/tcp.h>

#define LISTEN_PORT 8080
#define BUFLEN 4096

static char recvbuf[BUFLEN];
static unsigned long request_count = 0;
static struct timeval start_time;
static int connection_errors = 0;

__attribute__((always_inline))
static inline void get_uptime(struct timeval *uptime) {
    struct timeval now;
    gettimeofday(&now, NULL);
    uptime->tv_sec = now.tv_sec - start_time.tv_sec;
    uptime->tv_usec = now.tv_usec - start_time.tv_usec;
    if (uptime->tv_usec < 0) {
        uptime->tv_sec--;
        uptime->tv_usec += 1000000;
    }
}

__attribute__((always_inline))
static inline void parse_path(const char *req, char *path, size_t maxlen) {
    path[0] = '/'; path[1] = '\0';
    if (memcmp(req, "GET ", 4) != 0) return;

    const char *p = req + 4;
    const char *end = p;
    while (*end && *end != ' ') end++;

    size_t len = (size_t)(end - p);
    if (len > 0 && len < maxlen) {
        memcpy(path, p, len);
        path[len] = '\0';
    }
}

static const char favicon_response[] =
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Length: 0\r\n"
    "Connection: close\r\n\r\n";

static const char html_body_template[] =
    "<html><body>"
    "<h1>Unikraft HTTP Server</h1>"
    "<p>This is a unikernel running a minimal HTTP server!</p>"
    "<h2>Server Statistics:</h2>"
    "<ul>"
    "<li>Uptime: %ld.%06ld seconds</li>"
    "<li>Requests: %lu</li>"
    "<li>Errors: %d</li>"
    "<li>Path: %s</li>"
    "</ul>"
    "<p>URLs: <a href=\"/\">Home</a> | <a href=\"/stats\">Stats</a></p>"
    "</body></html>";

__attribute__((always_inline))
static inline void generate_response(char *buf, size_t *out_len, const char *path) {
    struct timeval uptime;
    get_uptime(&uptime);

    char *p = buf;

    if (__builtin_expect(strcmp(path, "/favicon.ico") == 0, 0)) {
        memcpy(buf, favicon_response, sizeof(favicon_response) - 1);
        *out_len = sizeof(favicon_response) - 1;
        return;
    }

    if (__builtin_expect(strcmp(path, "/stats") == 0, 0)) {
        p += sprintf(p,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: ");

        char *body_start = p;
        p += sprintf(p, "\r\nConnection: close\r\n\r\n");

        int header_len = p - buf;
        int json_len = sprintf(p,
            "{\n"
            "  \"uptime\": %ld.%06ld,\n"
            "  \"requests\": %lu,\n"
            "  \"errors\": %d\n"
            "}\n",
            uptime.tv_sec, uptime.tv_usec, request_count, connection_errors);

        sprintf(body_start - 2, "%d", json_len); // backfill Content-Length
        p += json_len;
        *out_len = p - buf;
        return;
    }

    char *html_start = p + 512;
    int html_len = sprintf(html_start, html_body_template,
        uptime.tv_sec, uptime.tv_usec, request_count, connection_errors, path);

    p += sprintf(p,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n\r\n",
        html_len);

    memcpy(p, html_start, html_len);
    p += html_len;
    *out_len = p - buf;
}

int main(void) {
    int srv, client;
    ssize_t n;
    struct sockaddr_in srv_addr;
    char response[BUFLEN];
    size_t response_len;
    char path[128];

    gettimeofday(&start_time, NULL);

    srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) return 1;

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = INADDR_ANY;
    srv_addr.sin_port = htons(LISTEN_PORT);

    if (bind(srv, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0)
        return 1;
    if (listen(srv, 16) < 0) return 1;

    for (;;) {
        client = accept(srv, NULL, 0);
        if (client < 0) {
            connection_errors++;
            continue;
        }

        int flag = 1;
        setsockopt(client, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        n = read(client, recvbuf, BUFLEN);
        if (n <= 0) {
            connection_errors++;
            close(client);
            continue;
        }

        request_count++;
        parse_path(recvbuf, path, sizeof(path));
        generate_response(response, &response_len, path);

        if (send(client, response, response_len, 0) < 0)
            connection_errors++;

        close(client);
    }

    close(srv);
    return 0;
}
