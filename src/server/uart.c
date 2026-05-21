#include "uart.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>

static int uart_fd = -1;

int uart_init(const char *port, int baud_rate) {
    uart_fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (uart_fd < 0) {
        perror("Failed to open UART device");
        return -1;
    }

    speed_t baud;
    switch (baud_rate) {
        case 230400: baud = B230400; break;
        case 460800: baud = B460800; break;
        case 921600: baud = B921600; break;
        default:     baud = B115200; break;
    }

    struct termios tty;
    if (tcgetattr(uart_fd, &tty) != 0) {
        perror("tcgetattr failed");
        close(uart_fd);
        uart_fd = -1;
        return -1;
    }

    cfsetospeed(&tty, baud);
    cfsetispeed(&tty, baud);

    tty.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS);
    tty.c_cflag |= CS8 | CREAD | CLOCAL;
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_oflag &= ~OPOST;
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 10;

    if (tcsetattr(uart_fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr failed");
        close(uart_fd);
        uart_fd = -1;
        return -1;
    }

    printf("[UART] Initialized %s at %d baud\n", port, baud_rate);
    return 0;
}

void uart_send(const Message *msg) {
    if (uart_fd < 0 || !msg) return;
    if (write(uart_fd, msg, sizeof(Message)) != (ssize_t)sizeof(Message)) {
        fprintf(stderr, "[UART] Write error: %s\n", strerror(errno));
    }
}

void uart_cleanup(void) {
    if (uart_fd >= 0) {
        close(uart_fd);
        uart_fd = -1;
    }
}
