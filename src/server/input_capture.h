#ifndef INPUT_CAPTURE_H
#define INPUT_CAPTURE_H

#include <stdint.h>

typedef struct {
    uint16_t type;
    uint16_t code;
    int32_t  value;
} InputEvent;

/* Scan /dev/input/event* and grab all keyboard/mouse devices.
 * Returns 0 on success, -1 if no devices found. */
int input_capture_init(void);

/* Add and grab a device by path. Returns the new fd, or -1 if not added
 * (already tracked, not a keyboard/mouse, or is our own uinput device). */
int input_capture_add_device(const char *path);

/* Ungrab and remove a device by path. No-op if not tracked. */
void input_capture_remove_device(const char *path);

/* Copy currently tracked fds into fds[]. Returns count. */
int input_capture_get_fds(int *fds, int max_fds);

/* Read one event from the device that owns fd.
 * Returns 0 on success (event filled), -1 when no more events.
 * Handles ENODEV internally (removes disconnected device, closes fd). */
int input_capture_read_fd(int fd, InputEvent *event);

void input_capture_cleanup(void);

#endif // INPUT_CAPTURE_H
