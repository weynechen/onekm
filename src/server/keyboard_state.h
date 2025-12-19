#ifndef KEYBOARD_STATE_H
#define KEYBOARD_STATE_H

#include <stdint.h>
#include "common/protocol.h"

void keyboard_state_init(void);

int keyboard_state_process_key(uint16_t linux_keycode, uint8_t value, HIDKeyboardReport *report);

void keyboard_state_reset(HIDKeyboardReport *report);

#define MODIFIER_LEFT_CTRL   0x01
#define MODIFIER_LEFT_SHIFT  0x02
#define MODIFIER_LEFT_ALT    0x04
#define MODIFIER_LEFT_GUI    0x08
#define MODIFIER_RIGHT_CTRL  0x10
#define MODIFIER_RIGHT_SHIFT 0x20
#define MODIFIER_RIGHT_ALT   0x40
#define MODIFIER_RIGHT_GUI   0x80

#endif // KEYBOARD_STATE_H
