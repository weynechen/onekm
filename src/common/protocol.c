#include "protocol.h"

void msg_mouse_move(Message *msg, int16_t dx, int16_t dy) {
    if (msg) {
        msg->type = MSG_MOUSE_MOVE;
        msg->a = dx;
        msg->b = dy;
    }
}

void msg_mouse_button(Message *msg, uint8_t button, uint8_t state) {
    if (msg) {
        msg->type = MSG_MOUSE_BUTTON;
        msg->a = button;
        msg->b = state;
    }
}

void msg_key_event(Message *msg, uint16_t keycode, uint8_t state) {
    if (msg) {
        msg->type = MSG_KEY_EVENT;
        msg->a = (int16_t)keycode;
        msg->b = state;
    }
}

void msg_switch(Message *msg, uint8_t state) {
    if (msg) {
        msg->type = MSG_SWITCH;
        msg->a = state;
        msg->b = 0;
    }
}

void msg_parse(const Message *msg, uint8_t *type, int16_t *a, int16_t *b) {
    if (msg) {
        if (type) *type = msg->type;
        if (a) *a = msg->a;
        if (b) *b = msg->b;
    }
}