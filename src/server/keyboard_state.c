#include "keyboard_state.h"
#include <string.h>

// Linux evdev键码到HID键码的映射表
static const uint8_t linux_to_hid_keymap[256] = {
    [0] = 0,
    [1] = 41,      // KEY_ESC -> ESC
    [2] = 30,      // KEY_1 -> 1
    [3] = 31,      // KEY_2 -> 2
    [4] = 32,      // KEY_3 -> 3
    [5] = 33,      // KEY_4 -> 4
    [6] = 34,      // KEY_5 -> 5
    [7] = 35,      // KEY_6 -> 6
    [8] = 36,      // KEY_7 -> 7
    [9] = 37,      // KEY_8 -> 8
    [10] = 38,     // KEY_9 -> 9
    [11] = 39,     // KEY_0 -> 0
    [12] = 45,     // KEY_MINUS -> -
    [13] = 46,     // KEY_EQUAL -> =
    [14] = 42,     // KEY_BACKSPACE -> Backspace
    [15] = 43,     // KEY_TAB -> Tab
    [16] = 20,     // KEY_Q -> q
    [17] = 26,     // KEY_W -> w
    [18] = 8,      // KEY_E -> e
    [19] = 21,     // KEY_R -> r
    [20] = 23,     // KEY_T -> t
    [21] = 28,     // KEY_Y -> y
    [22] = 24,     // KEY_U -> u
    [23] = 12,     // KEY_I -> i
    [24] = 18,     // KEY_O -> o
    [25] = 19,     // KEY_P -> p
    [26] = 47,     // KEY_LEFTBRACE -> [
    [27] = 48,     // KEY_RIGHTBRACE -> ]
    [28] = 40,     // KEY_ENTER -> Enter
    [29] = 224,    // KEY_LEFTCTRL -> LCtrl
    [30] = 4,      // KEY_A -> a
    [31] = 22,     // KEY_S -> s
    [32] = 7,      // KEY_D -> d
    [33] = 9,      // KEY_F -> f
    [34] = 10,     // KEY_G -> g
    [35] = 11,     // KEY_H -> h
    [36] = 13,     // KEY_J -> j
    [37] = 14,     // KEY_K -> k
    [38] = 15,     // KEY_L -> l
    [39] = 51,     // KEY_SEMICOLON -> ;
    [40] = 52,     // KEY_APOSTROPHE -> '
    [41] = 53,     // KEY_GRAVE -> `
    [42] = 225,    // KEY_LEFTSHIFT -> LShift
    [43] = 49,     // KEY_BACKSLASH -> backslash
    [44] = 29,     // KEY_Z -> z
    [45] = 27,     // KEY_X -> x
    [46] = 6,      // KEY_C -> c
    [47] = 25,     // KEY_V -> v
    [48] = 5,      // KEY_B -> b
    [49] = 17,     // KEY_N -> n
    [50] = 16,     // KEY_M -> m
    [51] = 54,     // KEY_COMMA -> ,
    [52] = 55,     // KEY_DOT -> .
    [53] = 56,     // KEY_SLASH -> /
    [54] = 229,    // KEY_RIGHTSHIFT -> RShift
    [55] = 0,      // KEY_KPASTERISK -> *
    [56] = 226,    // KEY_LEFTALT -> LAlt
    [57] = 44,     // KEY_SPACE -> Space
    [58] = 57,     // KEY_CAPSLOCK -> Caps Lock
    [59] = 58,     // KEY_F1 -> F1
    [60] = 59,     // KEY_F2 -> F2
    [61] = 60,     // KEY_F3 -> F3
    [62] = 61,     // KEY_F4 -> F4
    [63] = 62,     // KEY_F5 -> F5
    [64] = 63,     // KEY_F6 -> F6
    [65] = 64,     // KEY_F7 -> F7
    [66] = 65,     // KEY_F8 -> F8
    [67] = 66,     // KEY_F9 -> F9
    [68] = 67,     // KEY_F10 -> F10
    [87] = 58,     // KEY_F11 -> F11
    [88] = 59,     // KEY_F12 -> F12
    [96] = 88,     // KEY_KPENTER -> Keypad Enter
    [97] = 228,    // KEY_RIGHTCTRL -> RCtrl
    [98] = 84,     // KEY_KPSLASH -> Keypad /
    [100] = 230,   // KEY_RIGHTALT -> RAlt
    [102] = 74,    // KEY_HOME -> Home
    [103] = 82,    // KEY_UP -> Up Arrow
    [104] = 75,    // KEY_PAGEUP -> Page Up
    [105] = 80,    // KEY_LEFT -> Left Arrow
    [106] = 79,    // KEY_RIGHT -> Right Arrow
    [107] = 77,    // KEY_END -> End
    [108] = 81,    // KEY_DOWN -> Down Arrow
    [109] = 78,    // KEY_PAGEDOWN -> Page Down
    [110] = 73,    // KEY_INSERT -> Insert
    [111] = 76     // KEY_DELETE -> Delete
};

// 当前键盘状态
static HIDKeyboardReport current_report = {0};

void keyboard_state_init(void) {
    memset(&current_report, 0, sizeof(HIDKeyboardReport));
}

// 查找按键在数组中的位置
static int find_key_in_report(uint8_t keycode) {
    for (int i = 0; i < 6; i++) {
        if (current_report.keys[i] == keycode) {
            return i;
        }
    }
    return -1;
}

// 从报告中移除按键
static void remove_key_from_report(uint8_t keycode) {
    int pos = find_key_in_report(keycode);
    if (pos >= 0) {
        // 将后面的按键前移
        for (int i = pos; i < 5; i++) {
            current_report.keys[i] = current_report.keys[i + 1];
        }
        current_report.keys[5] = 0;
    }
}

// 向报告中添加按键
static int add_key_to_report(uint8_t keycode) {
    // 如果已经存在，直接返回
    if (find_key_in_report(keycode) >= 0) {
        return 0;
    }

    // 查找空槽位
    for (int i = 0; i < 6; i++) {
        if (current_report.keys[i] == 0) {
            current_report.keys[i] = keycode;
            return 1;
        }
    }

    // 没有空槽位
    return -1;
}

int keyboard_state_process_key(uint16_t linux_keycode, uint8_t value, HIDKeyboardReport *report) {
    if (!report || linux_keycode >= 256) {
        return 0;
    }

    uint8_t hid_keycode = linux_to_hid_keymap[linux_keycode];

    // 如果没有映射，忽略
    if (hid_keycode == 0 && linux_keycode != 57) {  // 57 是空格键
        return 0;
    }

    int state_changed = 0;

    // 判断是否为修饰键
    switch (hid_keycode) {
        case 224:  // LCtrl
            if (value) current_report.modifiers |= MODIFIER_LEFT_CTRL;
            else current_report.modifiers &= ~MODIFIER_LEFT_CTRL;
            state_changed = 1;
            break;
        case 228:  // RCtrl
            if (value) current_report.modifiers |= MODIFIER_RIGHT_CTRL;
            else current_report.modifiers &= ~MODIFIER_RIGHT_CTRL;
            state_changed = 1;
            break;
        case 225:  // LShift
            if (value) current_report.modifiers |= MODIFIER_LEFT_SHIFT;
            else current_report.modifiers &= ~MODIFIER_LEFT_SHIFT;
            state_changed = 1;
            break;
        case 229:  // RShift
            if (value) current_report.modifiers |= MODIFIER_RIGHT_SHIFT;
            else current_report.modifiers &= ~MODIFIER_RIGHT_SHIFT;
            state_changed = 1;
            break;
        case 226:  // LAlt
            if (value) current_report.modifiers |= MODIFIER_LEFT_ALT;
            else current_report.modifiers &= ~MODIFIER_LEFT_ALT;
            state_changed = 1;
            break;
        case 230:  // RAlt
            if (value) current_report.modifiers |= MODIFIER_RIGHT_ALT;
            else current_report.modifiers &= ~MODIFIER_RIGHT_ALT;
            state_changed = 1;
            break;
        default:
            // 普通按键
            if (value) {
                // 按键按下
                if (add_key_to_report(hid_keycode) > 0) {
                    state_changed = 1;
                }
            } else {
                // 按键释放
                remove_key_from_report(hid_keycode);
                state_changed = 1;
            }
            break;
    }

    // 如果状态改变，复制当前状态
    if (state_changed) {
        memcpy(report, &current_report, sizeof(HIDKeyboardReport));
        return 1;
    }

    return 0;
}

void keyboard_state_reset(HIDKeyboardReport *report) {
    memset(&current_report, 0, sizeof(HIDKeyboardReport));
    if (report) {
        memcpy(report, &current_report, sizeof(HIDKeyboardReport));
    }
}
