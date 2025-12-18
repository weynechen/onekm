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
        running = 0;
        printf("\nShutting down...\n");
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

    // Main loop
    while (running) {
        // Receive message from server
        int bytes_received = recv(sock_fd, (char*)&msg, sizeof(Message), 0);

        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                printf("\nServer disconnected\n");
            } else {
                printf("Error receiving data: %d\n", WSAGetLastError());
            }
            break;
        }

        if (bytes_received != sizeof(Message)) {
            printf("Received incomplete message (%d bytes)\n", bytes_received);
            continue;
        }

        // Debug: log received message
        printf("DEBUG: Received msg type=%d, a=%d, b=%d\n", msg.type, msg.a, msg.b);

        // Process message based on type
        switch (msg.type) {
            case MSG_MOUSE_MOVE:
                printf("DEBUG: Mouse move dx=%d, dy=%d\n", msg.a, msg.b);
                inject_mouse_move(msg.a, msg.b);
                break;

            case MSG_MOUSE_BUTTON:
                printf("DEBUG: Mouse button=%d, state=%d\n", msg.a, msg.b);
                inject_mouse_button(msg.a, msg.b);
                break;

            case MSG_KEY_EVENT:
                {
                    WORD vk_code = map_scancode_to_vk(msg.a);
                    printf("DEBUG: Key event scancode=%d -> vk_code=%d, state=%d\n", msg.a, vk_code, msg.b);
                    if (vk_code != 0) {
                        inject_key_event(vk_code, msg.b);
                    } else {
                        printf("DEBUG: Unknown scancode %d, ignoring\n", msg.a);
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
    }

    // Cleanup
    printf("\nCleaning up...\n");
    cleanup_input_inject();
    closesocket(sock_fd);
    WSACleanup();

    printf("Client shutdown complete\n");
    return 0;
}