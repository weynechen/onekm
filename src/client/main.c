#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include "common/protocol.h"
#include "input_inject.h"
#include "keymap.h"

#pragma comment(lib, "ws2_32.lib")

static int running = 1;

BOOL WINAPI console_handler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
        printf("\nReceived shutdown signal (code: %d)\n", signal);
        running = 0;
        return TRUE;
    }
    return FALSE;
}

int main(int argc, char *argv[]) {
    WSADATA wsaData;
    SOCKET sock_fd;
    struct sockaddr_in server_addr;
    Message msg;
    char *server_ip;
    int server_port = 24800;

    printf("LanKM Client v1.0.0\n");

    // Setup console handler
    SetConsoleCtrlHandler(console_handler, TRUE);

    // Check arguments
    if (argc < 2) {
        printf("Usage: %s <server-ip> [port]\n", argv[0]);
        printf("Example: %s 192.168.1.100\n", argv[0]);
        return 1;
    }

    server_ip = argv[1];
    if (argc > 2) {
        server_port = atoi(argv[2]);
        if (server_port <= 0 || server_port > 65535) {
            printf("Invalid port number. Using default port 24800.\n");
            server_port = 24800;
        }
    }

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Failed to initialize Winsock: %d\n", WSAGetLastError());
        return 1;
    }

    // Create socket
    sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_fd == INVALID_SOCKET) {
        printf("Failed to create socket: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // Disable Nagle's algorithm for low latency
    BOOL nagle_disabled = TRUE;
    if (setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, (char*)&nagle_disabled, sizeof(nagle_disabled)) == SOCKET_ERROR) {
        printf("Warning: Could not disable Nagle's algorithm\n");
    }

    // Disable socket send buffer to reduce buffering
    int sendbuf_size = 0; // Use system default but hint for minimal buffering
    if (setsockopt(sock_fd, SOL_SOCKET, SO_SNDBUF, (char*)&sendbuf_size, sizeof(sendbuf_size)) == SOCKET_ERROR) {
        printf("Warning: Could not set send buffer size\n");
    }

    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    // Convert IP address
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) != 1) {
        printf("Invalid IP address: %s\n", server_ip);
        closesocket(sock_fd);
        WSACleanup();
        return 1;
    }

    // Connect to server
    printf("Connecting to %s:%d...\n", server_ip, server_port);
    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Failed to connect to server: %d\n", WSAGetLastError());
        closesocket(sock_fd);
        WSACleanup();
        return 1;
    }

    printf("Connected to server!\n");

    // Initialize input injection
    if (init_input_inject() != 0) {
        printf("Failed to initialize input injection\n");
        closesocket(sock_fd);
        WSACleanup();
        return 1;
    }

    // Initialize keymap
    init_keymap();

    printf("Receiving input events... Press Ctrl+C to disconnect\n");

    // Set socket to non-blocking mode
    u_long mode = 1;  // 1 = non-blocking
    ioctlsocket(sock_fd, FIONBIO, &mode);

    // Main loop
    while (running) {
        // Use select() to check if data is available
        fd_set read_fds;
        struct timeval timeout;

        FD_ZERO(&read_fds);
        FD_SET(sock_fd, &read_fds);

        // Set timeout to 1ms for low latency (was 50ms)
        // We still need a timeout to allow checking the running flag periodically
        timeout.tv_sec = 0;
        timeout.tv_usec = 1000;  // 1ms

        int result = select(0, &read_fds, NULL, NULL, &timeout);

        if (result > 0 && FD_ISSET(sock_fd, &read_fds)) {
            // Data is available, receive it
            // Try to receive multiple messages in batch for efficiency
            int batch_count = 0;
            while (batch_count < 10) { // Process up to 10 messages per iteration
                int bytes_received = recv(sock_fd, (char*)&msg, sizeof(Message), 0);

                if (bytes_received <= 0) {
                    int error = WSAGetLastError();
                    if (bytes_received == 0) {
                        printf("\nServer disconnected\n");
                        running = 0; // Exit main loop
                        break;
                    } else if (error == WSAEWOULDBLOCK) {
                        // No more data available right now, exit batch loop gracefully
                        break;
                    } else if (error == WSAECONNRESET) {
                        printf("\nConnection reset by server\n");
                        running = 0;
                        break;
                    } else {
                        printf("Error receiving data: %d\n", error);
                        running = 0;
                        break;
                    }
                }

                if (bytes_received != sizeof(Message)) {
                    printf("Received incomplete message (%d bytes)\n", bytes_received);
                    break;
                }

                // Process message based on type
                switch (msg.type) {
                    case MSG_MOUSE_MOVE:
                        inject_mouse_move(msg.a, msg.b);
                        break;

                    case MSG_MOUSE_BUTTON:
                        inject_mouse_button(msg.a, msg.b);
                        break;

                    case MSG_KEY_EVENT:
                        {
                            WORD vk_code = map_scancode_to_vk(msg.a);
                            if (vk_code != 0) {
                                inject_key_event(vk_code, msg.b);
                            }
                        }
                        break;

                    case MSG_SWITCH:
                        if (msg.a == 1) {
                            printf("Control switched to remote\n");
                        } else {
                            printf("Control switched to local\n");
                        }
                        break;

                    default:
                        printf("Unknown message type: %d\n", msg.type);
                        break;
                }

                batch_count++;

                // Quick check if more data is available without waiting
                fd_set read_fds_quick;
                struct timeval timeout_quick = {0, 0}; // Non-blocking check
                FD_ZERO(&read_fds_quick);
                FD_SET(sock_fd, &read_fds_quick);

                if (select(0, &read_fds_quick, NULL, NULL, &timeout_quick) <= 0) {
                    break; // No more data immediately available
                }
            }
        } else if (result == SOCKET_ERROR) {
            printf("select() error: %d\n", WSAGetLastError());
            break;
        }
        // If result == 0, timeout occurred, just loop again to check running flag
        // No extra sleep needed - we already have 1ms timeout in select()
    }

    // Cleanup
    printf("\nCleaning up...\n");

    // Graceful shutdown: set socket back to blocking mode for proper close
    mode = 0;  // 0 = blocking
    ioctlsocket(sock_fd, FIONBIO, &mode);

    // Clean up resources
    cleanup_input_inject();

    // Close socket gracefully
    shutdown(sock_fd, SD_BOTH);
    closesocket(sock_fd);
    WSACleanup();

    printf("Client shutdown complete\n");
    return 0;
}