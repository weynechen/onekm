#include "input_inject.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>

static INPUT input_buffer[2];
static int input_count = 0;

// Screen dimensions for absolute positioning
static int screen_width = 0;
static int screen_height = 0;
static int current_x = 0;
static int current_y = 0;

// Helper function to get screen dimensions
void update_screen_dimensions(void) {
    screen_width = GetSystemMetrics(SM_CXSCREEN);
    screen_height = GetSystemMetrics(SM_CYSCREEN);

    // Initialize cursor position to center if not set
    if (current_x == 0 && current_y == 0) {
        current_x = screen_width / 2;
        current_y = screen_height / 2;
    }
}

// Helper function to set absolute mouse position
void set_mouse_position_absolute(int x, int y) {
    // Normalize to 0-65535 range for absolute coordinates
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dx = (x * 65535) / screen_width;
    input.mi.dy = (y * 65535) / screen_height;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    input.mi.mouseData = 0;
    input.mi.time = 0;
    input.mi.dwExtraInfo = 0;

    SendInput(1, &input, sizeof(INPUT));
}

int init_input_inject(void) {
    // Initialize input buffer
    memset(input_buffer, 0, sizeof(input_buffer));
    input_count = 0;

    // No need for complex initialization with relative positioning
    return 0;
}

void inject_mouse_move(int dx, int dy) {
    // Use relative positioning for better performance
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;  // Use relative move
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