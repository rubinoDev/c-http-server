// REFERENCES:
// https://beej.us/guide/bgnet/
// https://www.reddit.com/r/C_Programming/comments/kbfa6t/building_a_http_server_in_c/
// https://datatracker.ietf.org/doc/html/rfc1945

#include <sys/types.h>
// Provides basic data types used in system calls, such as:
//   - pid_t, uid_t, gid_t, ssize_t, off_t, etc.
// In this code: used by socket-related functions (e.g., accept(), send(), recv()).

#include <sys/socket.h>
// Provides socket API functions and constants:
//   - socket(), bind(), listen(), accept(), send(), recv(), setsockopt()
//   - Structures: struct sockaddr, struct sockaddr_storage
// In this code: all the main TCP server functionality (creating socket, listening, accepting connections)

#include <netdb.h>
// Provides network database operations and utilities:
//   - getaddrinfo(), freeaddrinfo(), gai_strerror()
//   - struct addrinfo
// In this code: used to resolve port and address info to bind the server socket (IPv4/IPv6 agnostic)

#include <stdio.h>
// Provides standard I/O functions:
//   - printf(), fprintf(), perror(), snprintf(), fopen(), fread(), fclose()
// In this code: printing logs, errors, building response strings, reading files

#include <string.h>
// Provides string manipulation functions:
//   - strlen(), strcmp(), strncmp(), strstr(), strchr(), memset(), memcpy()
// In this code: parsing HTTP requests, handling paths, cleaning buffers, comparing strings

#include <stdlib.h>
// Provides general utilities:
//   - malloc(), free(), exit(), atoi(), system()
// In this code: dynamic memory allocation for file content, exiting on fatal errors

#include <unistd.h>
// Provides POSIX API functions:
//   - close(), read(), write(), getpid(), fork(), sleep()
// In this code: closing sockets, sending data, interacting with file descriptors

#include <limits.h>
// Provides system limits constants:
//   - PATH_MAX, INT_MAX, LONG_MAX, etc.
// In this code: defining maximum allowed path length for safe file path operations

#define BACKLOG 10
#define MAXDATASIZE 4096

// -------------------------------------------
// Helper: Send a complete HTTP response
// -------------------------------------------
void send_response(int fd, int status_code, const char *status_text,
                   const char *content_type, const char *body)
{
    char header[512];
    int body_len = body ? strlen(body) : 0;

    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.0 %d %s\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %d\r\n"
                              "\r\n",
                              status_code, status_text, content_type, body_len);

    send(fd, header, header_len, 0);

    if (body && body_len > 0)
        send(fd, body, body_len, 0);
}

// -------------------------------------------
// Helper: Send an error response (JSON body)
// -------------------------------------------
void send_error(int fd, int code, const char *message)
{
    char body[256];
    snprintf(body, sizeof(body), "{\"error\": \"%s\"}", message);

    switch (code)
    {
    case 400:
        send_response(fd, 400, "Bad Request", "application/json", body);
        break;
    case 403:
        send_response(fd, 403, "Forbidden", "application/json", body);
        break;
    case 404:
        send_response(fd, 404, "Not Found", "application/json", body);
        break;
    case 500:
        send_response(fd, 500, "Internal Server Error", "application/json", body);
        break;
    default:
        send_response(fd, code, "Error", "application/json", body);
    }
}

// -------------------------------------------
// Helper: Read file content into memory
// -------------------------------------------
char *read_file(const char *file_path, long *out_size)
{
    FILE *file = fopen(file_path, "rb");
    if (!file)
        return NULL;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char *buffer = malloc(size + 1);
    if (!buffer)
    {
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, size, file);
    fclose(file);

    if (bytes_read != size)
    {
        free(buffer);
        return NULL;
    }

    buffer[size] = '\0';
    *out_size = size;
    return buffer;
}

// -------------------------------------------
// Helper: Detect Content-Type from file extension
// -------------------------------------------
const char *get_content_type(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext)
        return "text/plain";
    if (strcmp(ext, ".html") == 0)
        return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(ext, ".png") == 0)
        return "image/png";
    if (strcmp(ext, ".css") == 0)
        return "text/css";
    if (strcmp(ext, ".js") == 0)
        return "application/javascript";
    return "application/octet-stream";
}

// -------------------------------------------
// Handle a single client connection
// -------------------------------------------
void handle_client(int new_fd, const char *root_dir)
{
    char buf[MAXDATASIZE];
    int numbytes = recv(new_fd, buf, MAXDATASIZE - 1, 0);

    if (numbytes <= 0)
    {
        close(new_fd);
        return;
    }

    buf[numbytes] = '\0';

    char method[8], path[256], protocol[16];
    if (sscanf(buf, "%7s %255s %15s", method, path, protocol) != 3)
    {
        send_error(new_fd, 400, "Malformed request");
        close(new_fd);
        return;
    }

    // Remove query string and fragments
    char *qmark = strchr(path, '?');
    if (qmark)
        *qmark = '\0';
    char *hash = strchr(path, '#');
    if (hash)
        *hash = '\0';

    // Basic security: block path traversal
    if (strstr(path, ".."))
    {
        send_error(new_fd, 403, "Forbidden path traversal");
        close(new_fd);
        return;
    }

    // Only support GET requests
    if (strcmp(method, "GET") != 0)
    {
        send_error(new_fd, 501, "Only GET is supported");
        close(new_fd);
        return;
    }

    // Default file (index.html)
    char requested_path[PATH_MAX];
    if (strcmp(path, "/") == 0)
        snprintf(requested_path, sizeof(requested_path), "%s/index.html", root_dir);
    else
        snprintf(requested_path, sizeof(requested_path), "%s%s", root_dir, path);

    // Resolve absolute path
    char real_requested_path[PATH_MAX];
    if (!realpath(requested_path, real_requested_path))
    {
        send_error(new_fd, 404, "File not found");
        close(new_fd);
        return;
    }

    // Resolve absolute root directory
    char real_root[PATH_MAX];
    realpath(root_dir, real_root);

    // Check if requested file is inside root_dir (prevent traversal)
    if (strncmp(real_requested_path, real_root, strlen(real_root)) != 0)
    {
        send_error(new_fd, 403, "Forbidden path");
        close(new_fd);
        return;
    }

    // Read file
    long file_size = 0;
    char *body = read_file(real_requested_path, &file_size);
    if (!body)
    {
        send_error(new_fd, 404, "File not found");
        close(new_fd);
        return;
    }

    // Send the file with correct Content-Type
    const char *content_type = get_content_type(real_requested_path);
    char header[512];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.0 200 OK\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %ld\r\n"
                              "\r\n",
                              content_type, file_size);

    // Send header + body
    send(new_fd, header, header_len, 0);

    ssize_t sent = send(new_fd, body, file_size, 0);
    if (sent == -1)
        send_error(new_fd, 500, "Failed to send response body");

    free(body);
    close(new_fd);
}

// -------------------------------------------
// Main server setup and loop
// -------------------------------------------
int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s <port> <root_directory>\n", argv[0]);
        exit(1);
    }

    const char *port = argv[1];
    const char *root_dir = argv[2];

    int sockfd;
    struct addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // Use my IP

    int status = getaddrinfo(NULL, port, &hints, &servinfo);
    if (status != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    // Try all results until one works
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1)
        {
            perror("socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            perror("bind");
            close(sockfd);
            continue;
        }

        if (listen(sockfd, BACKLOG) == -1)
        {
            perror("listen");
            close(sockfd);
            continue;
        }

        printf("✅ Server listening on port %s\n", port);

        struct sockaddr_storage their_addr;
        socklen_t addr_size = sizeof(their_addr);

        while (1)
        {
            int new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
            if (new_fd == -1)
            {
                perror("accept");
                continue;
            }

            printf("💻 Client connected!\n");
            handle_client(new_fd, root_dir);
        }

        break;
    }

    if (!p)
    {
        fprintf(stderr, "Failed to bind socket\n");
        exit(2);
    }

    freeaddrinfo(servinfo);
    close(sockfd);
    return 0;
}
