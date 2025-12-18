#ifndef NETWORK_H
#define NETWORK_H

#include "protocol.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VAL INVALID_SOCKET
    #define SOCKET_ERROR_VAL SOCKET_ERROR
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    typedef int socket_t;
    #define INVALID_SOCKET_VAL -1
    #define SOCKET_ERROR_VAL -1
    #define closesocket close
#endif

typedef struct {
    socket_t sock;
    struct sockaddr_in addr;
} NetworkConnection;

// Network initialization and cleanup
int network_init(void);
void network_cleanup(void);

// Server functions
socket_t server_create_socket(int port);
int server_listen(socket_t sock);
socket_t server_accept(socket_t sock, NetworkConnection *conn);

// Client functions
socket_t client_connect(const char *host, int port);

// Message functions
int send_message(socket_t sock, const Message *msg);
int recv_message(socket_t sock, Message *msg);

// Utility functions
int set_socket_nonblocking(socket_t sock);
int set_socket_timeout(socket_t sock, int seconds);
void close_connection(socket_t sock);

#endif // NETWORK_H