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

static inline void parse_path(const char *req, char *path, size_t maxlen) {
    path[0] = '/'; path[1] = '\0'; // default to "/"
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

static inline void generate_response(char *buf, size_t *out_len, const char *path) {
    struct timeval uptime;
    get_uptime(&uptime);

    char *p = buf;

    if (strcmp(path, "/favicon.ico") == 0) {
        p += sprintf(p,
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n");
        *out_len = p - buf;
        return;
    }

    if (strcmp(path, "/stats") == 0) {
        p += sprintf(p,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Connection: close\r\n");

        int header_len = p - buf;
        p += sprintf(p,
            "\r\n"
            "{\n"
            "  \"uptime\": %ld.%06ld,\n"
            "  \"requests\": %lu,\n"
            "  \"errors\": %d\n"
            "}\n",
            uptime.tv_sec, uptime.tv_usec, request_count, connection_errors);
        
        size_t body_len = (p - buf) - header_len;
        memmove(buf + header_len - 2, buf + header_len, body_len);  // remove extra \r\n
        p = buf + header_len - 2 + body_len;
        *out_len = p - buf;
        return;
    }

    // default: HTML
    const char *html_body_fmt =
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

    char *html_start = p + 512;
    int html_len = sprintf(html_start, html_body_fmt,
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
    int srv, client, rc;
    ssize_t n;
    struct sockaddr_in srv_addr;
    char response[BUFLEN];
    size_t response_len;
    char path[128];

    gettimeofday(&start_time, NULL);
    printf("Minimal HTTP unikernel server starting...\n");

    srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) return 1;

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = INADDR_ANY;
    srv_addr.sin_port = htons(LISTEN_PORT);

    if (bind(srv, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0)
        return 1;
    if (listen(srv, 1) < 0) return 1;

    printf("Listening on port %d...\n", LISTEN_PORT);

    while (1) {
        client = accept(srv, NULL, 0);
        if (client < 0) {
            connection_errors++;
            continue;
        }

        // Disable Nagle
        int flag = 1;
        setsockopt(client, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        memset(recvbuf, 0, BUFLEN);
        read(client, recvbuf, BUFLEN);

        request_count++;
        parse_path(recvbuf, path, sizeof(path));
        generate_response(response, &response_len, path);
        
        n = send(client, response, response_len, 0);
        if (n < 0) connection_errors++;

        close(client);
    }

    close(srv);
    return 0;
}
