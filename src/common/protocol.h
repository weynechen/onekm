#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#pragma pack(push, 1)

// USB HID键盘报告（8字节）
typedef struct {
    uint8_t modifiers;     // 修饰键位掩码
    uint8_t reserved;      // 保留（必须为0）
    uint8_t keys[6];       // 最多6个同时按下的按键
} HIDKeyboardReport;

// 统一的二进制消息格式
typedef struct {
    uint8_t type;          // 消息类型
    union {
        struct {
            int16_t dx;    // 鼠标X位移
            int16_t dy;    // 鼠标Y位移
        } mouse_move;
        struct {
            uint8_t button; // 鼠标按键（1=左，2=右，3=中）
            uint8_t state;  // 状态（0=释放，1=按下）
            uint8_t padding[2]; // 填充
        } mouse_button;
        HIDKeyboardReport keyboard; // 键盘HID报告（8字节）
        struct {
            uint8_t state;  // 控制状态（0=本地，1=远程）
            uint8_t padding[3]; // 填充
        } control;
        struct {
            int16_t vertical;   // 垂直滚轮（通常为正=向上，负=向下）
            int16_t horizontal; // 水平滚轮（通常为正=向右，负=向左）
        } mouse_wheel;
    } data;
} Message;

#pragma pack(pop)

// 消息类型定义
enum MessageType {
    MSG_MOUSE_MOVE = 0x01,
    MSG_MOUSE_BUTTON = 0x02,
    MSG_KEYBOARD_REPORT = 0x03,  // 发送完整的HID键盘报告
    MSG_SWITCH = 0x04,
    MSG_MOUSE_WHEEL = 0x05       // 鼠标滚轮事件
};

// 鼠标按键定义
enum MouseButton {
    MOUSE_BUTTON_LEFT = 0x01,
    MOUSE_BUTTON_RIGHT = 0x02,
    MOUSE_BUTTON_MIDDLE = 0x03
};

// 按键状态
enum ButtonState {
    BUTTON_RELEASED = 0,
    BUTTON_PRESSED = 1
};

// 控制状态
enum ControlState {
    CONTROL_LOCAL = 0,
    CONTROL_REMOTE = 1
};

// 修饰键定义
#define MODIFIER_LEFT_CTRL   0x01
#define MODIFIER_LEFT_SHIFT  0x02
#define MODIFIER_LEFT_ALT    0x04
#define MODIFIER_LEFT_GUI    0x08
#define MODIFIER_RIGHT_CTRL  0x10
#define MODIFIER_RIGHT_SHIFT 0x20
#define MODIFIER_RIGHT_ALT   0x40
#define MODIFIER_RIGHT_GUI   0x80

// Message construction functions
void msg_mouse_move(Message *msg, int16_t dx, int16_t dy);
void msg_mouse_button(Message *msg, uint8_t button, uint8_t state);
void msg_keyboard_report(Message *msg, const HIDKeyboardReport *report);
void msg_switch(Message *msg, uint8_t state);
void msg_mouse_wheel(Message *msg, int16_t vertical, int16_t horizontal);

// Legacy function (removed - no longer needed)
// void msg_key_event(Message *msg, uint16_t keycode, uint8_t state);

#endif // PROTOCOL_H