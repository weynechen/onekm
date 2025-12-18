#include "keymap.h"
#include <windows.h>

// Key mapping from Linux scancodes to Windows virtual key codes
typedef struct {
    uint16_t linux_code;
    uint16_t windows_vk;
} KeyMapping;

// Standard key mapping from Linux evdev key codes to Windows virtual key codes
static const KeyMapping keymap[] = {
    // Escape and function keys
    {1, VK_ESCAPE},
    {59, VK_F1}, {60, VK_F2}, {61, VK_F3}, {62, VK_F4},
    {63, VK_F5}, {64, VK_F6}, {65, VK_F7}, {66, VK_F8},
    {67, VK_F9}, {68, VK_F10}, {87, VK_F11}, {88, VK_F12},

    // Row 1 (numbers and symbols)
    {2, '1'}, {3, '2'}, {4, '3'}, {5, '4'}, {6, '5'},
    {7, '6'}, {8, '7'}, {9, '8'}, {10, '9'}, {11, '0'},
    {12, VK_OEM_MINUS}, {13, VK_OEM_PLUS}, {14, VK_BACK},
    {15, VK_TAB}, {51, VK_OEM_5},

    // Row 2 (QWERTY)
    {16, 'Q'}, {17, 'W'}, {18, 'E'}, {19, 'R'}, {20, 'T'},
    {21, 'Y'}, {22, 'U'}, {23, 'I'}, {24, 'O'}, {25, 'P'},
    {26, VK_OEM_4}, {27, VK_OEM_6}, {43, VK_RETURN},

    // Row 3 (ASDF)
    {30, 'A'}, {31, 'S'}, {32, 'D'}, {33, 'F'}, {34, 'G'},
    {35, 'H'}, {36, 'J'}, {37, 'K'}, {38, 'L'}, {39, VK_OEM_1},
    {40, VK_OEM_7}, {41, VK_OEM_3}, {86, VK_OEM_102},

    // Row 4 (ZXCV)
    {44, 'Z'}, {45, 'X'}, {46, 'C'}, {47, 'V'}, {48, 'B'},
    {49, 'N'}, {50, 'M'}, {51, VK_OEM_COMMA}, {52, VK_OEM_PERIOD},
    {53, VK_OEM_2}, {54, VK_RSHIFT},

    // Row 5 (modifiers and space)
    {29, VK_CONTROL}, {42, VK_LSHIFT}, {56, VK_LMENU}, {57, VK_SPACE},
    {58, VK_CAPITAL}, {69, VK_NUMLOCK}, {70, VK_SCROLL}, {83, VK_DELETE},

    // Arrow keys and navigation block
    {71, VK_HOME}, {72, VK_UP}, {73, VK_PRIOR}, {74, VK_LEFT},
    {75, VK_CLEAR}, {76, VK_RIGHT}, {77, VK_END}, {78, VK_DOWN},
    {79, VK_NEXT}, {82, VK_INSERT},

    // Numpad
    {82, VK_NUMPAD0}, {79, VK_NUMPAD1}, {80, VK_NUMPAD2},
    {81, VK_NUMPAD3}, {75, VK_NUMPAD4}, {76, VK_NUMPAD5},
    {77, VK_NUMPAD6}, {71, VK_NUMPAD7}, {72, VK_NUMPAD8},
    {73, VK_NUMPAD9}, {55, VK_MULTIPLY}, {74, VK_SUBTRACT},
    {78, VK_ADD}, {96, VK_SEPARATOR}, {98, VK_RETURN}, {110, VK_DECIMAL},

    // Modifiers (right side)
    {97, VK_CONTROL}, {100, VK_RMENU}, {110, VK_DECIMAL},

    // Windows keys
    {125, VK_LWIN}, {126, VK_RWIN}, {127, VK_APPS},

    // Special keys
    {119, VK_PAUSE}, {120, VK_SNAPSHOT},
    {121, VK_EXECUTE}, {122, VK_HELP}, {123, VK_MENU}, {124, VK_SELECT},
};

static const int keymap_size = sizeof(keymap) / sizeof(KeyMapping);

void init_keymap(void) {
    // No initialization needed for static keymap
}

uint16_t map_scancode_to_vk(uint16_t linux_code) {
    // Search for matching key
    for (int i = 0; i < keymap_size; i++) {
        if (keymap[i].linux_code == linux_code) {
            return keymap[i].windows_vk;
        }
    }

    // Key not found in map
    printf("Warning: Unmapped Linux key code: %d\n", linux_code);
    return 0;
}