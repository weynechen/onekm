#include "protocol.h"
#include <string.h>

void msg_mouse_move(Message *msg, int16_t dx, int16_t dy) {
    if (msg) {
        msg->type = MSG_MOUSE_MOVE;
        msg->data.mouse_move.dx = dx;
        msg->data.mouse_move.dy = dy;
    }
}

void msg_mouse_button(Message *msg, uint8_t button, uint8_t state) {
    if (msg) {
        msg->type = MSG_MOUSE_BUTTON;
        msg->data.mouse_button.button = button;
        msg->data.mouse_button.state = state;
    }
}

void msg_keyboard_report(Message *msg, const HIDKeyboardReport *report) {
    if (msg && report) {
        msg->type = MSG_KEYBOARD_REPORT;
        memcpy(&msg->data.keyboard, report, sizeof(HIDKeyboardReport));
    }
}

void msg_switch(Message *msg, uint8_t state) {
    if (msg) {
        msg->type = MSG_SWITCH;
        msg->data.control.state = state;
    }
}

void msg_mouse_wheel(Message *msg, int16_t vertical, int16_t horizontal) {
    if (msg) {
        msg->type = MSG_MOUSE_WHEEL;
        msg->data.mouse_wheel.vertical = vertical;
        msg->data.mouse_wheel.horizontal = horizontal;
    }
}