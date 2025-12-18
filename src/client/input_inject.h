#ifndef INPUT_INJECT_H
#define INPUT_INJECT_H

#include <stdint.h>

int init_input_inject(void);
void inject_mouse_move(int dx, int dy);
void inject_mouse_button(uint8_t button, uint8_t state);
void inject_key_event(uint16_t vk_code, uint8_t state);
void cleanup_input_inject(void);

#endif // INPUT_INJECT_H