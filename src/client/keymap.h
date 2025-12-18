#ifndef KEYMAP_H
#define KEYMAP_H

#include <stdint.h>

void init_keymap(void);
uint16_t map_scancode_to_vk(uint16_t linux_code);

#endif // KEYMAP_H