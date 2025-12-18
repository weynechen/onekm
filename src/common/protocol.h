#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#ifdef _WIN32
    #pragma pack(push, 1)
#else
    #pragma pack(1)
#endif

typedef struct {
    uint8_t type;
    int16_t a;
    int16_t b;
} Message;

#ifdef _WIN32
    #pragma pack(pop)
#endif

enum MessageType {
    MSG_MOUSE_MOVE = 0x01,
    MSG_MOUSE_BUTTON = 0x02,
    MSG_KEY_EVENT = 0x03,
    MSG_SWITCH = 0x04
};

enum MouseButton {
    MOUSE_BUTTON_LEFT = 0x01,
    MOUSE_BUTTON_RIGHT = 0x02,
    MOUSE_BUTTON_MIDDLE = 0x03
};

enum ButtonState {
    BUTTON_RELEASED = 0,
    BUTTON_PRESSED = 1
};

enum ControlState {
    CONTROL_LOCAL = 0,
    CONTROL_REMOTE = 1
};

// Linux evdev key codes (selected commonly used ones)
enum LinuxKeyCode {
    KEY_ESC = 1,
    KEY_1 = 2,
    KEY_2 = 3,
    KEY_3 = 4,
    KEY_4 = 5,
    KEY_5 = 6,
    KEY_6 = 7,
    KEY_7 = 8,
    KEY_8 = 9,
    KEY_9 = 10,
    KEY_0 = 11,
    KEY_MINUS = 12,
    KEY_EQUAL = 13,
    KEY_BACKSPACE = 14,
    KEY_TAB = 15,
    KEY_Q = 16,
    KEY_W = 17,
    KEY_E = 18,
    KEY_R = 19,
    KEY_T = 20,
    KEY_Y = 21,
    KEY_U = 22,
    KEY_I = 23,
    KEY_O = 24,
    KEY_P = 25,
    KEY_LEFTBRACE = 26,
    KEY_RIGHTBRACE = 27,
    KEY_ENTER = 28,
    KEY_LEFTCTRL = 29,
    KEY_A = 30,
    KEY_S = 31,
    KEY_D = 32,
    KEY_F = 33,
    KEY_G = 34,
    KEY_H = 35,
    KEY_J = 36,
    KEY_K = 37,
    KEY_L = 38,
    KEY_SEMICOLON = 39,
    KEY_APOSTROPHE = 40,
    KEY_GRAVE = 41,
    KEY_LEFTSHIFT = 42,
    KEY_BACKSLASH = 43,
    KEY_Z = 44,
    KEY_X = 45,
    KEY_C = 46,
    KEY_V = 47,
    KEY_B = 48,
    KEY_N = 49,
    KEY_M = 50,
    KEY_COMMA = 51,
    KEY_DOT = 52,
    KEY_SLASH = 53,
    KEY_RIGHTSHIFT = 54,
    KEY_KPASTERISK = 55,
    KEY_LEFTALT = 56,
    KEY_SPACE = 57,
    KEY_CAPSLOCK = 58,
    KEY_F1 = 59,
    KEY_F2 = 60,
    KEY_F3 = 61,
    KEY_F4 = 62,
    KEY_F5 = 63,
    KEY_F6 = 64,
    KEY_F7 = 65,
    KEY_F8 = 66,
    KEY_F9 = 67,
    KEY_F10 = 68,
    KEY_NUMLOCK = 69,
    KEY_SCROLLLOCK = 70,
    KEY_KP7 = 71,
    KEY_KP8 = 72,
    KEY_KP9 = 73,
    KEY_KPMINUS = 74,
    KEY_KP4 = 75,
    KEY_KP5 = 76,
    KEY_KP6 = 77,
    KEY_KPPLUS = 78,
    KEY_KP1 = 79,
    KEY_KP2 = 80,
    KEY_KP3 = 81,
    KEY_KP0 = 82,
    KEY_KPDOT = 83,
    KEY_F11 = 87,
    KEY_F12 = 88,
    KEY_KPENTER = 96,
    KEY_RIGHTCTRL = 97,
    KEY_KPSLASH = 98,
    KEY_RIGHTALT = 100,
    KEY_HOME = 102,
    KEY_UP = 103,
    KEY_PAGEUP = 104,
    KEY_LEFT = 105,
    KEY_RIGHT = 106,
    KEY_END = 107,
    KEY_DOWN = 108,
    KEY_PAGEDOWN = 109,
    KEY_INSERT = 110,
    KEY_DELETE = 111
};

// Message construction functions
void msg_mouse_move(Message *msg, int16_t dx, int16_t dy);
void msg_mouse_button(Message *msg, uint8_t button, uint8_t state);
void msg_key_event(Message *msg, uint16_t keycode, uint8_t state);
void msg_switch(Message *msg, uint8_t state);

// Message parsing functions
void msg_parse(const Message *msg, uint8_t *type, int16_t *a, int16_t *b);

// Network constants
#define LANKM_PORT 24800
#define MSG_SIZE sizeof(Message)

#endif // PROTOCOL_H