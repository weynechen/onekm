#ifndef UART_H
#define UART_H

#include "common/protocol.h"

int  uart_init(const char *port, int baud_rate);
void uart_send(const Message *msg);
void uart_cleanup(void);

#endif // UART_H
