# Simple HTTP Server in C

A minimal HTTP server written in C for educational purposes. This project demonstrates how TCP sockets, HTTP requests, and file serving work under the hood. It supports basic **GET requests**, serves files from a root directory, and handles errors such as 400, 403, 404, and 500.

## Features

- TCP server using `socket()`, `bind()`, `listen()`, `accept()`.  
- Handles HTTP GET requests and serves files from a specified root directory.  
- Prevents path traversal attacks (`../`) to improve security.  
- Automatically resolves nested paths and symbolic links.  
- Returns appropriate HTTP error responses in JSON:  
  - `400 Bad Request`  
  - `403 Forbidden`  
  - `404 Not Found`  
  - `500 Internal Server Error`  
- Detects common content types (`.html`, `.jpg`, `.png`, `.css`, `.js`).  

## Getting Started

### Prerequisites

- GCC or any C compiler  
- POSIX-compatible OS (Linux, macOS)  
- Basic terminal knowledge  

### Build

```bash
gcc -o simple_http_server main.c
```
Or if you have multiple source files:

```
bash
gcc -o simple_http_server src/*.c
```
Run

```
bash
./simple_http_server <port> <root_directory>
```
Example:

```
bash
./simple_http_server 8080 ./www
```

This will start the server on port 8080 and serve files from the www folder.

Usage
Open a browser or use curl to test:

```
bash
curl http://localhost:8080/index.html
```

Access / → serves index.html

Access /image.jpg → serves image.jpg from the root directory

Requests to files outside root (path traversal) will return 403 Forbidden

Notes
This is a learning project. It does not implement full HTTP/1.1 features, SSL/TLS, or advanced request handling.

Designed to teach the fundamentals of networking and HTTP servers in C.

Inspired by Beej's Guide to Network Programming: https://beej.us/guide/bgnet/

License
This project is open-source and available under the MIT License.
