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
#include <sched.h>
#include <pthread.h>
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
int init_uart(const char *port, int baud_rate) {
    struct termios tty;
    speed_t baud;

    uart_fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (uart_fd < 0) {
        perror("Failed to open UART device");
        return -1;
    }

    // Convert baud rate to termios constant
    switch (baud_rate) {
        case 230400: baud = B230400; break;
        case 460800: baud = B460800; break;
        case 921600: baud = B921600; break;
        default: baud = B115200; break;
    }

    // Get current terminal settings
    if (tcgetattr(uart_fd, &tty) != 0) {
        perror("tcgetattr failed");
        close(uart_fd);
        return -1;
    }

    // Set baud rate
    cfsetospeed(&tty, baud);
    cfsetispeed(&tty, baud);

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

    printf("UART initialized: %s at %d baud\n", port, baud_rate);
    return 0;
}

// Send binary message to ESP32
void send_message(Message *msg) {
    if (uart_fd >= 0 && msg) {

        // switch (msg->type) {
        //     case 1: // MSG_MOUSE_MOVE
        //         printf(" dx=%d dy=%d", msg->data.mouse_move.dx, msg->data.mouse_move.dy);
        //         break;
        //     case 2: // MSG_MOUSE_BUTTON
        //         printf(" button=%d state=%d", msg->data.mouse_button.button, msg->data.mouse_button.state);
        //         break;
        //     case 3: // MSG_KEYBOARD_REPORT
        //         printf(" keyboard modifiers=0x%02x keys=[%02x %02x %02x %02x %02x %02x]",
        //                msg->data.keyboard.modifiers,
        //                msg->data.keyboard.keys[0], msg->data.keyboard.keys[1],
        //                msg->data.keyboard.keys[2], msg->data.keyboard.keys[3],
        //                msg->data.keyboard.keys[4], msg->data.keyboard.keys[5]);
        //         break;
        //     case 4: // MSG_SWITCH
        //         printf(" switch state=%d", msg->data.control.state);
        //         break;
        // }
        // printf("\n");

        int sent = write(uart_fd, msg, sizeof(Message));
        if (sent != sizeof(Message)) {
            fprintf(stderr, "UART write error: %s\n", strerror(errno));
        }
    }
}

int main(int argc, char *argv[]) {
    Message msg;
    const char *uart_port = "/dev/ttyACM0";
    int baud_rate = 230400;  // Default to 230400

    // Parse command line arguments
    if (argc > 1) {
        uart_port = argv[1];
    }
    if (argc > 2) {
        baud_rate = atoi(argv[2]);
        if (baud_rate != 115200 && baud_rate != 230400 &&
            baud_rate != 460800 && baud_rate != 921600) {
            fprintf(stderr, "Warning: Unsupported baud rate %d, using 230400\n", baud_rate);
            baud_rate = 230400;
        }
    }

    printf("LanKM Server v2.0.0 (UART Mode)\n");
    printf("Using UART device: %s at %d baud\n", uart_port, baud_rate);

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
    if (init_uart(uart_port, baud_rate) != 0) {
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

    // Mouse movement flush timer
    struct timespec last_mouse_flush = {0, 0};

    while (running) {
        int events_processed = 0;
        struct timespec current_ts;
        clock_gettime(CLOCK_MONOTONIC, &current_ts);

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

        // Flush pending mouse movement if no events for a while
        if (events_processed == 0) {
            // Check if we should flush pending mouse movement (after 5ms of inactivity)
            long time_since_flush = (current_ts.tv_sec - last_mouse_flush.tv_sec) * 1000000 +
                                   (current_ts.tv_nsec - last_mouse_flush.tv_nsec) / 1000;
            if (time_since_flush > 5000) { // 5ms
                if (flush_pending_mouse_movement(&msg)) {
                    send_message(&msg);
                    events_processed++; // Count this as activity to avoid sleep
                }
                last_mouse_flush = current_ts;
            }
        } else {
            // Update flush timer when events are processed
            last_mouse_flush = current_ts;
        }

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