#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <linux/input.h>
#include <time.h>
#include "common/protocol.h"
#include "input_capture.h"
#include "state_machine.h"
#include "keyboard_state.h"

// F12键码（用于切换LOCAL/REMOTE模式）
#define KEY_F12 88

static int running = 1;
static int uart_fd = -1;
static struct termios saved_termios;  // 保存原始的终端设置

// 设置终端为raw模式
static void set_raw_terminal_mode(void) {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &saved_termios);
    raw = saved_termios;
    raw.c_lflag &= ~(ECHO | ICANON);  // 清除ECHO和ICANON
    raw.c_lflag |= ISIG;  // 显式确保ISIG被设置，允许Ctrl+C
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// 恢复终端设置
static void restore_terminal_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
}

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        running = 0;
        printf("\nShutting down...\n");
    }
}

// Emergency cleanup handler - called on abnormal termination
void emergency_cleanup(void) {
    printf("\nEmergency cleanup - releasing all input devices...\n");
    // 恢复终端
    restore_terminal_mode();
    // Force ungrab all devices
    set_device_grab(0);
    cleanup_input_capture();

    if (uart_fd >= 0) {
        close(uart_fd);
    }
}

// Open and configure UART device
int init_uart(const char *port) {
    struct termios tty;

    uart_fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (uart_fd < 0) {
        perror("Failed to open UART device");
        return -1;
    }

    // Get current terminal settings
    if (tcgetattr(uart_fd, &tty) != 0) {
        perror("tcgetattr failed");
        close(uart_fd);
        return -1;
    }

    // Set baud rate
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    // 8N1 configuration
    tty.c_cflag &= ~PARENB;        // No parity
    tty.c_cflag &= ~CSTOPB;        // 1 stop bit
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;            // 8 data bits
    tty.c_cflag &= ~CRTSCTS;       // No hardware flow control
    tty.c_cflag |= CREAD | CLOCAL; // Enable receiver, ignore modem control

    // Raw mode
    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ECHONL;
    tty.c_lflag &= ~ISIG;

    // Disable software flow control
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    // Raw output
    tty.c_oflag &= ~OPOST;

    // Timeout settings
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10;  // 1 second timeout

    // Apply settings
    if (tcsetattr(uart_fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr failed");
        close(uart_fd);
        return -1;
    }

    printf("UART initialized: %s at 115200 baud\n", port);
    return 0;
}

// Send binary message to ESP32
void send_message(Message *msg) {
    if (uart_fd >= 0 && msg) {
        int sent = write(uart_fd, msg, sizeof(Message));
        if (sent != sizeof(Message)) {
            fprintf(stderr, "UART write error: %s\n", strerror(errno));
        }
    }
}

int main(int argc, char *argv[]) {
    Message msg;
    const char *uart_port = "/dev/ttyACM0";

    // Parse command line arguments
    if (argc > 1) {
        uart_port = argv[1];
    }

    printf("LanKM Server v2.0.0 (UART Mode)\n");
    printf("Using UART device: %s\n", uart_port);

    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Register emergency cleanup
    atexit(emergency_cleanup);

    // Initialize input capture
    if (init_input_capture() != 0) {
        fprintf(stderr, "Failed to initialize input capture\n");
        return 1;
    }

    // Initialize state machine
    init_state_machine();

    // Initialize keyboard state
    keyboard_state_init();

    // Initialize UART
    if (init_uart(uart_port) != 0) {
        fprintf(stderr, "Failed to initialize UART\n");
        cleanup_input_capture();
        return 1;
    }

    printf("Ready. Press F12 to toggle LOCAL/REMOTE mode\n");
    printf("Press Ctrl+C to shutdown\n");

    // 设置终端为raw模式
    set_raw_terminal_mode();
    printf("Terminal set to raw mode (Ctrl+C enabled)\n");

    // Main loop - optimized for low latency
    HIDKeyboardReport keyboard_report;
    int last_report_sent = 0;

    // Heartbeat timer for LOCAL mode
    time_t last_heartbeat = 0;
    int heartbeat_delay = 0;  // Counter for delay between press and release
    const time_t heartbeat_interval = 30; // Send heartbeat every 30 seconds
    const int heartbeat_key_delay = 1000; // About 100ms delay (1000 * 0.1ms)

    while (running) {
        int events_processed = 0;

        // Check for heartbeat in LOCAL mode
        if (get_current_state() == STATE_LOCAL) {
            time_t current_time = time(NULL);

            if (last_heartbeat == 0) {
                // Initialize heartbeat timer
                last_heartbeat = current_time;
            } else if (heartbeat_delay > 0) {
                // In the delay period between press and release
                heartbeat_delay--;

                // Send release when delay is finished
                if (heartbeat_delay == 0) {
                    memset(&keyboard_report, 0, sizeof(HIDKeyboardReport));
                    msg_keyboard_report(&msg, &keyboard_report);
                    send_message(&msg);
                }

                events_processed++; // Count this as activity to avoid sleep
            } else if (current_time - last_heartbeat >= heartbeat_interval) {
                // Time to send heartbeat - press Shift key
                memset(&keyboard_report, 0, sizeof(HIDKeyboardReport));
                keyboard_report.keys[0] = 225; // Left Shift key HID code
                msg_keyboard_report(&msg, &keyboard_report);
                send_message(&msg);

                // Set delay before release (about 100ms)
                heartbeat_delay = heartbeat_key_delay;
                last_heartbeat = current_time;
                events_processed++;
            }
        }

        // Capture and process multiple events in quick succession
        for (int i = 0; i < 20; i++) {
            InputEvent event;
            if (capture_input(&event) == 0) {
                // Process through state machine
                if (process_event(&event, &msg)) {
                    // Send message
                    send_message(&msg);
                    events_processed++;
                } else if (get_current_state() == STATE_REMOTE && event.type == EV_KEY) {
                    // 处理键盘事件，在REMOTE模式下
                    if (keyboard_state_process_key(event.code, event.value, &keyboard_report)) {
                        // 键盘状态改变，发送HID报告
                        msg_keyboard_report(&msg, &keyboard_report);
                        send_message(&msg);
                        last_report_sent = 1;
                        events_processed++;
                    }
                }
            } else {
                break; // No more events available
            }
        }

        // 如果有键盘状态改变但未发送（比如快速连续按键），立即发送
        if (get_current_state() == STATE_REMOTE && !last_report_sent) {
            // 这里可以根据需要添加逻辑
        }
        last_report_sent = 0;

        // If no events were processed, sleep briefly to avoid busy-waiting
        if (events_processed == 0) {
            usleep(100); // 0.1ms sleep when idle
        }
    }

    // Cleanup
    cleanup_state_machine();
    cleanup_input_capture();

    if (uart_fd >= 0) {
        close(uart_fd);
    }

    printf("Server shutdown complete\n");
    return 0;
}