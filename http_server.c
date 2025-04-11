/* Improved HTTP server unikernel with better connection handling */
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

#define LISTEN_PORT 8080
#define BUFLEN 4096
#define MAX_CLIENTS 10

static char recvbuf[BUFLEN];
static unsigned long request_count = 0;
static struct timeval start_time;
static int connection_errors = 0;

// Performance metrics
void print_uptime() {
    struct timeval now, diff;
    gettimeofday(&now, NULL);
    
    diff.tv_sec = now.tv_sec - start_time.tv_sec;
    diff.tv_usec = now.tv_usec - start_time.tv_usec;
    if (diff.tv_usec < 0) {
        diff.tv_sec--;
        diff.tv_usec += 1000000;
    }
    
    printf("Uptime: %ld.%06ld seconds\n", diff.tv_sec, diff.tv_usec);
}

// Get request path (very basic parser)
void get_request_path(const char *request, char *path, size_t max_len) {
    path[0] = '\0';
    
    // Look for the GET path
    const char *get_pos = strstr(request, "GET ");
    if (get_pos) {
        get_pos += 4; // Skip "GET "
        const char *space_pos = strchr(get_pos, ' ');
        if (space_pos) {
            size_t path_len = space_pos - get_pos;
            if (path_len < max_len) {
                strncpy(path, get_pos, path_len);
                path[path_len] = '\0';
            } else {
                strncpy(path, get_pos, max_len - 1);
                path[max_len - 1] = '\0';
            }
        }
    }
    
    // Default to root if no path found
    if (path[0] == '\0') {
        strcpy(path, "/");
    }
}

// HTTP response with dynamic content including metrics
void generate_response(char *buffer, size_t *len, const char *path) {
    struct timeval now;
    gettimeofday(&now, NULL);
    
    // Get uptime
    struct timeval diff;
    diff.tv_sec = now.tv_sec - start_time.tv_sec;
    diff.tv_usec = now.tv_usec - start_time.tv_usec;
    if (diff.tv_usec < 0) {
        diff.tv_sec--;
        diff.tv_usec += 1000000;
    }
    
    char temp[BUFLEN];
    
    // Handle different paths
    if (strcmp(path, "/favicon.ico") == 0) {
        // Return a 404 for favicon to avoid the double counting in browsers
        *len = snprintf(buffer, BUFLEN - 1,
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n"
            "\r\n");
        return;
    } 
    else if (strcmp(path, "/stats") == 0) {
        // JSON stats for programmatic access
        snprintf(temp, BUFLEN - 1,
            "{\n"
            "  \"uptime\": %ld.%06ld,\n"
            "  \"requests\": %lu,\n"
            "  \"errors\": %d\n"
            "}\n",
            diff.tv_sec, diff.tv_usec, request_count, connection_errors);
    }
    else {
        // Default HTML page
        snprintf(temp, BUFLEN - 1,
            "<html><body>\n"
            "<h1>Unikraft HTTP Server</h1>\n"
            "<p>This is a unikernel running a minimal HTTP server!</p>\n"
            "<h2>Server Statistics:</h2>\n"
            "<ul>\n"
            "  <li>Server Uptime: %ld.%06ld seconds</li>\n"
            "  <li>Requests Handled: %lu</li>\n"
            "  <li>Connection Errors: %d</li>\n"
            "  <li>Current Path: %s</li>\n"
            "</ul>\n"
            "<p>Available URLs:</p>\n"
            "<ul>\n"
            "  <li><a href=\"/\">Home Page</a></li>\n"
            "  <li><a href=\"/stats\">Stats JSON</a></li>\n"
            "</ul>\n"
            "<p>Refresh to see the request count increase!</p>\n"
            "</body></html>",
            diff.tv_sec, diff.tv_usec, request_count, connection_errors, path);
    }
    
    *len = snprintf(buffer, BUFLEN - 1,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s", 
        (strcmp(path, "/stats") == 0) ? "application/json" : "text/html",
        strlen(temp), temp);
}

int main(void) {
    int rc = 0;
    int srv, client;
    ssize_t n;
    struct sockaddr_in srv_addr;
    char response[BUFLEN];
    size_t response_len;
    char path[256];

    // Record start time for uptime calculation
    gettimeofday(&start_time, NULL);
    
    printf("Initializing HTTP server unikernel\n");

    srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        fprintf(stderr, "Failed to create socket: %d\n", errno);
        goto out;
    }
    
    // Set socket options for better connection handling
    int option = 1;
    if (setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0) {
        fprintf(stderr, "Failed to set socket options: %d\n", errno);
    }

    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = INADDR_ANY;
    srv_addr.sin_port = htons(LISTEN_PORT);

    rc = bind(srv, (struct sockaddr *) &srv_addr, sizeof(srv_addr));
    if (rc < 0) {
        fprintf(stderr, "Failed to bind socket: %d\n", errno);
        goto out;
    }

    rc = listen(srv, MAX_CLIENTS);
    if (rc < 0) {
        fprintf(stderr, "Failed to listen on socket: %d\n", errno);
        goto out;
    }

    printf("HTTP server listening on port %d...\n", LISTEN_PORT);
    
    while (1) {
        client = accept(srv, NULL, 0);
        if (client < 0) {
            fprintf(stderr, "Failed to accept connection: %d\n", errno);
            connection_errors++;
            continue;
        }

        /* Receive request (ignore errors) */
        memset(recvbuf, 0, BUFLEN);
        read(client, recvbuf, BUFLEN);
        
        /* Increment request counter */
        request_count++;
        
        /* Parse request path */
        get_request_path(recvbuf, path, sizeof(path));
        
        /* Log request info */
        printf("Request #%lu received for path: %s\n", request_count, path);
        
        /* Generate and send response */
        generate_response(response, &response_len, path);
        n = write(client, response, response_len);
        
        if (n < 0) {
            fprintf(stderr, "Failed to send a reply\n");
            connection_errors++;
        } else {
            printf("Sent a reply (%zd bytes)\n", n);
            print_uptime();
        }

        /* Close connection */
        close(client);
    }

out:
    if (srv >= 0)
        close(srv);
    return rc;
}