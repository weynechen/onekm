#include "input_inject.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>

static INPUT input_buffer[2];
static int input_count = 0;

int init_input_inject(void) {
    // Initialize input buffer
    memset(input_buffer, 0, sizeof(input_buffer));
    input_count = 0;
    return 0;
}

void inject_mouse_move(int dx, int dy) {
    static int current_x = 0;
    static int current_y = 0;

    current_x += dx;
    current_y += dy;

    // Get current mouse position
    POINT pt;
    GetCursorPos(&pt);

    // Set new position
    pt.x += dx;
    pt.y += dy;

    // Create mouse move input
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dx = pt.x;
    input.mi.dy = pt.y;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    input.mi.mouseData = 0;
    input.mi.time = 0;
    input.mi.dwExtraInfo = 0;

    SendInput(1, &input, sizeof(INPUT));
}

void inject_mouse_button(uint8_t button, uint8_t state) {
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dx = 0;
    input.mi.dy = 0;
    input.mi.mouseData = 0;
    input.mi.time = 0;
    input.mi.dwExtraInfo = 0;

    switch (button) {
        case 1: // Left button
            input.mi.dwFlags = (state == 1) ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
            break;
        case 2: // Right button
            input.mi.dwFlags = (state == 1) ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
            break;
        case 3: // Middle button
            input.mi.dwFlags = (state == 1) ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
            break;
        default:
            return; // Unsupported button
    }

    SendInput(1, &input, sizeof(INPUT));
}

void inject_key_event(uint16_t vk_code, uint8_t state) {
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk_code;
    input.ki.wScan = 0;
    input.ki.dwFlags = (state == 1) ? 0 : KEYEVENTF_KEYUP;
    input.ki.time = 0;
    input.ki.dwExtraInfo = 0;

    // Handle special keys that need extended flag
    switch (vk_code) {
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_MENU:
        case VK_LMENU:
        case VK_RMENU:
        case VK_INSERT:
        case VK_DELETE:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_NUMLOCK:
        case VK_CANCEL:
        case VK_SNAPSHOT:
        case VK_DIVIDE:
            input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
            break;
    }

    SendInput(1, &input, sizeof(INPUT));
}

void cleanup_input_inject(void) {
    // Nothing to cleanup
}