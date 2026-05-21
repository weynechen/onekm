#ifndef UINPUT_INJECT_H
#define UINPUT_INJECT_H

#include <stdint.h>

/* Create a virtual keyboard+mouse via /dev/uinput.
 * Local X11 session receives input through this device. */
int  uinput_inject_init(void);

/* Write a single input event to the virtual device. */
void uinput_inject_event(uint16_t type, uint16_t code, int32_t value);

void uinput_inject_cleanup(void);

#endif // UINPUT_INJECT_H
