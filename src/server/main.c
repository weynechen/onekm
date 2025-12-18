#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include "common/protocol.h"
#include "input_capture.h"
#include "edge_detector.h"
#include "state_machine.h"

static int running = 1;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        running = 0;
        printf("\nShutting down...\n");
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    Message msg;

    printf("LanKM Server v1.0.0\n");

    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize input capture
    if (init_input_capture() != 0) {
        fprintf(stderr, "Failed to initialize input capture\n");
        return 1;
    }

    // Initialize edge detector
    if (init_edge_detector() != 0) {
        fprintf(stderr, "Failed to initialize edge detector\n");
        cleanup_input_capture();
        return 1;
    }

    // Initialize state machine
    init_state_machine();

    // Create TCP socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Failed to create socket");
        cleanup_edge_detector();
        cleanup_input_capture();
        return 1;
    }

    // Set socket options
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind to port 24800
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(24800);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to bind to port");
        close(server_fd);
        cleanup_edge_detector();
        cleanup_input_capture();
        return 1;
    }

    // Listen for connections
    if (listen(server_fd, 1) < 0) {
        perror("Failed to listen");
        close(server_fd);
        cleanup_edge_detector();
        cleanup_input_capture();
        return 1;
    }

    printf("Server listening on port 24800...\n");
    printf("Press Ctrl+C to shutdown\n");

    // Set server socket to non-blocking
    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    // Wait for client connection or shutdown signal
    client_fd = -1;
    while (running && client_fd < 0) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No connection waiting, check signal and sleep
                usleep(100000); // 100ms
                continue;
            } else {
                perror("Failed to accept connection");
                break;
            }
        }
    }

    if (!running) {
        printf("Shutdown requested, exiting...\n");
        close(server_fd);
        cleanup_edge_detector();
        cleanup_input_capture();
        return 0;
    }

    printf("Client connected from %s\n", inet_ntoa(client_addr.sin_addr));

    // Set client socket to non-blocking
    flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

    // Disable Nagle's algorithm for low latency
    int nagle_disabled = 1;
    if (setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nagle_disabled, sizeof(nagle_disabled)) < 0) {
        fprintf(stderr, "Warning: Could not disable Nagle's algorithm\n");
    }

    // Disable TCP Cork/Nagle for even lower latency
    #ifdef TCP_CORK
    int cork_disabled = 0;
    setsockopt(client_fd, IPPROTO_TCP, TCP_CORK, &cork_disabled, sizeof(cork_disabled));
    #endif

    // Main loop - optimized for low latency
    while (running) {
        int events_processed = 0;

        // Capture and process multiple events in quick succession to reduce latency
        // This allows processing multiple mouse movements without sleeping between them
        for (int i = 0; i < 20; i++) { // Process up to 20 events per batch
            InputEvent event;
            if (capture_input(&event) == 0) {
                // Process through state machine
                if (process_event(&event, &msg)) {
                    // Send message to client with MSG_NOSIGNAL to prevent SIGPIPE on connection issues
                    // This is more efficient and avoids crashes
                    ssize_t sent = send(client_fd, &msg, sizeof(Message), MSG_NOSIGNAL);
                    if (sent < 0) {
                        // Check if it's just a would-block error or actual connection issue
                        if (errno != EAGAIN && errno != EWOULDBLOCK) {
                            perror("Failed to send message");
                            running = 0;  // Exit loop on error
                            break;
                        }
                        // If would-block, we skip this event - input buffer on socket is full
                        // This is better than blocking and causing a backlog
                    }
                    events_processed++;
                }
            } else {
                break; // No more events available
            }
        }

        // If we processed events, don't sleep
        // If no events were processed, sleep briefly to avoid busy-waiting
        if (events_processed == 0) {
            usleep(100); // 0.1ms sleep when idle - reduced from 1ms
        }
    }

    // Cleanup
    close(client_fd);
    close(server_fd);
    cleanup_state_machine();
    cleanup_edge_detector();
    cleanup_input_capture();

    printf("Server shutdown complete\n");
    return 0;
}