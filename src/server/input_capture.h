#ifndef INPUT_CAPTURE_H
#define INPUT_CAPTURE_H

#include <stdint.h>

typedef struct {
    uint16_t type;
    uint16_t code;
    int32_t value;
} InputEvent;

int init_input_capture(void);
int capture_input(InputEvent *event);
void cleanup_input_capture(void);

#endif // INPUT_CAPTURE_H