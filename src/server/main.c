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
#include <linux/input-event-codes.h>
#include <time.h>
#include <sched.h>
#include <pthread.h>
#include <poll.h>
#include "common/protocol.h"
#include "input_capture.h"
#include "state_machine.h"
#include "keyboard_state.h"
#include "key_sync.h"

static int running = 1;
static int uart_fd = -1;
static struct termios saved_termios;

static void set_raw_terminal_mode(void) {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &saved_termios);
    raw = saved_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_lflag |= ISIG;
    raw.c_cc[VINTR] = 3;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void restore_terminal_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
}

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        running = 0;
        printf("\nShutting down...\n");
    }
}

void emergency_cleanup(void) {
    printf("\nEmergency cleanup - releasing all input devices...\n");
    restore_terminal_mode();
    set_device_grab(0);
    key_sync_cleanup();
    cleanup_input_capture();

    if (uart_fd >= 0) {
        close(uart_fd);
    }
}

int init_uart(const char *port, int baud_rate) {
    struct termios tty;
    speed_t baud;

    uart_fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (uart_fd < 0) {
        perror("Failed to open UART device");
        return -1;
    }

    switch (baud_rate) {
        case 230400: baud = B230400; break;
        case 460800: baud = B460800; break;
        case 921600: baud = B921600; break;
        default: baud = B115200; break;
    }

    if (tcgetattr(uart_fd, &tty) != 0) {
        perror("tcgetattr failed");
        close(uart_fd);
        return -1;
    }

    cfsetospeed(&tty, baud);
    cfsetispeed(&tty, baud);

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;

    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ECHONL;
    tty.c_lflag &= ~ISIG;

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10;

    if (tcsetattr(uart_fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr failed");
        close(uart_fd);
        return -1;
    }

    printf("UART initialized: %s at %d baud\n", port, baud_rate);
    return 0;
}

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
    int baud_rate = 230400;

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

    printf("OneKM Server v2.0.0 (UART Mode)\n");
    printf("Using UART device: %s at %d baud\n", uart_port, baud_rate);

    signal(SIGTERM, signal_handler);
    atexit(emergency_cleanup);

    if (init_input_capture() != 0) {
        fprintf(stderr, "Failed to initialize input capture\n");
        return 1;
    }

    init_state_machine();
    keyboard_state_init();

    if (key_sync_init() != 0) {
        fprintf(stderr, "Warning: Failed to initialize key sync module\n");
        fprintf(stderr, "Key synchronization will be disabled\n");
    }

    if (init_uart(uart_port, baud_rate) != 0) {
        fprintf(stderr, "Failed to initialize UART\n");
        key_sync_cleanup();
        cleanup_input_capture();
        return 1;
    }

    printf("Ready. Press PAUSE to toggle LOCAL/REMOTE mode\n");
    printf("Press PAUSE 3 times within 2 seconds to shutdown\n");

    set_raw_terminal_mode();
    printf("Terminal set to raw mode\n");

#define MAX_DEVICES 10

    HIDKeyboardReport keyboard_report;

    time_t last_heartbeat = 0;
    const time_t heartbeat_interval = 30;
    int heartbeat_mouse_moved = 0;
    int heartbeat_suspended = 0;  /* set when remote lock (Win+L) sent; cleared when entering REMOTE */

    struct timespec last_mouse_flush = {0, 0};

    int device_fds[MAX_DEVICES];
    int num_fds = get_device_fds(device_fds, MAX_DEVICES);

    struct pollfd pollfds[MAX_DEVICES];
    for (int i = 0; i < num_fds; i++) {
        pollfds[i].fd = device_fds[i];
        pollfds[i].events = POLLIN;
    }

    while (running) {
        if (should_exit()) {
            printf("Exit requested, shutting down...\n");
            break;
        }

        int events_processed = 0;
        ControlState current_state = get_current_state();

        /* Clear remote-lock heartbeat suspension when using remote again */
        if (current_state == STATE_REMOTE) {
            heartbeat_suspended = 0;
        }

        if (current_state == STATE_LOCAL) {
            time_t current_time = time(NULL);

            if (!heartbeat_suspended && heartbeat_mouse_moved > 0) {
                int xy_move = (heartbeat_mouse_moved % 2 == 0) ? 1 : -1;
                heartbeat_mouse_moved--;
                msg_mouse_move(&msg, xy_move, xy_move);
                send_message(&msg);
                events_processed++;

                if (heartbeat_mouse_moved == 0) {
                    printf("[HEARTBEAT] Mouse movement heartbeat sent\n");
                }
            } else if (!heartbeat_suspended && last_heartbeat == 0) {
                last_heartbeat = current_time;
            } else if (!heartbeat_suspended && current_time - last_heartbeat >= heartbeat_interval) {
                printf("[HEARTBEAT] Starting mouse movement heartbeat\n");
                heartbeat_mouse_moved = 5;
                last_heartbeat = current_time;
                events_processed++;
            }
        }

        if (current_state == STATE_REMOTE) {
            struct timespec current_ts;
            clock_gettime(CLOCK_MONOTONIC, &current_ts);

            for (int i = 0; i < 20; i++) {
                InputEvent event;
                int captured = capture_input(&event);
                if (captured == 0) {
                    if (event.type == EV_KEY && event.code == KEY_PAUSE && event.value == 1 &&
                        get_current_state() == STATE_LOCAL && heartbeat_mouse_moved > 0) {
                        printf("[HEARTBEAT] Canceling pending mouse movements before mode switch\n");
                        heartbeat_mouse_moved = 0;
                    }

                    if (process_event(&event, &msg)) {
                        send_message(&msg);
                        events_processed++;
                    } else if (get_current_state() == STATE_REMOTE && event.type == EV_KEY) {
                        if (keyboard_state_process_key(event.code, event.value, &keyboard_report)) {
                            msg_keyboard_report(&msg, &keyboard_report);
                            send_message(&msg);
                            events_processed++;
                        }
                    }
                } else {
                    break;
                }
            }

            if (events_processed == 0) {
                long time_since_flush = (current_ts.tv_sec - last_mouse_flush.tv_sec) * 1000000 +
                                       (current_ts.tv_nsec - last_mouse_flush.tv_nsec) / 1000;
                if (time_since_flush > 5000) {
                    if (flush_pending_mouse_movement(&msg)) {
                        send_message(&msg);
                        events_processed++;
                    }
                    last_mouse_flush = current_ts;
                }
            } else {
                last_mouse_flush = current_ts;
            }
        } else {
            if (poll(pollfds, num_fds, 1) > 0) {
                InputEvent event;
                if (capture_input(&event) == 0) {
                    if (event.type == EV_KEY && event.code == KEY_PAUSE && event.value == 1) {
                        process_event(&event, &msg);
                    } else if (event.type == EV_KEY && event.code == KEY_L && event.value == 1) {
                        /* Detect Win+L in LOCAL: send lock to remote and stop wake heartbeat */
                        uint8_t key_states[32];
                        if (get_hardware_keyboard_state(key_states) == 0) {
                            int win_pressed = (key_states[KEY_LEFTMETA / 8] >> (KEY_LEFTMETA % 8)) & 1;
                            win_pressed |= (key_states[KEY_RIGHTMETA / 8] >> (KEY_RIGHTMETA % 8)) & 1;
                            if (win_pressed) {
                                HIDKeyboardReport lock_press = {0};
                                lock_press.modifiers = MODIFIER_LEFT_GUI;
                                lock_press.keys[0] = 15;  /* HID usage for L */
                                msg_keyboard_report(&msg, &lock_press);
                                send_message(&msg);
                                events_processed++;
                                HIDKeyboardReport lock_release = {0};
                                msg_keyboard_report(&msg, &lock_release);
                                send_message(&msg);
                                events_processed++;
                                heartbeat_suspended = 1;
                                printf("[LOCK] Win+L sent to remote, wake heartbeat suspended\n");
                            }
                        }
                    }
                }
            }
        }

        int sleep_time;
        if (current_state == STATE_LOCAL) {
            sleep_time = heartbeat_mouse_moved > 0 ? 5 : 50;
        } else {
            sleep_time = events_processed > 0 ? 0 : 1;
        }

        if (events_processed == 0 && sleep_time > 0) {
            usleep(sleep_time * 1000);
        }
    }

    cleanup_state_machine();
    key_sync_cleanup();
    cleanup_input_capture();

    if (uart_fd >= 0) {
        close(uart_fd);
    }

    printf("Server shutdown complete\n");
    return 0;
}
