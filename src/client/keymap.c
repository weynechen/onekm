#include "keymap.h"
#include <windows.h>

// Key mapping from Linux scancodes to Windows virtual key codes
typedef struct {
    uint16_t linux_code;
    uint16_t windows_vk;
} KeyMapping;

// Standard key mapping from Linux evdev key codes to Windows virtual key codes
// This mapping prioritizes standard keyboard keys over numpad to avoid conflicts
static const KeyMapping keymap[] = {
    // --- Essential keys (fixes) ---
    {28, VK_RETURN},      // MAIN Enter key (CRITICAL FIX - was missing!)

    // --- Escape and Function keys ---
    {1, VK_ESCAPE},
    {59, VK_F1}, {60, VK_F2}, {61, VK_F3}, {62, VK_F4},
    {63, VK_F5}, {64, VK_F6}, {65, VK_F7}, {66, VK_F8},
    {67, VK_F9}, {68, VK_F10}, {87, VK_F11}, {88, VK_F12},

    // --- Number row ---
    {2, '1'}, {3, '2'}, {4, '3'}, {5, '4'}, {6, '5'},
    {7, '6'}, {8, '7'}, {9, '8'}, {10, '9'}, {11, '0'},
    {12, VK_OEM_MINUS}, {13, VK_OEM_PLUS},

    // --- Top row punctuation ---
    {26, VK_OEM_4},      // [
    {27, VK_OEM_6},      // ]
    {43, VK_OEM_5},      // \ (should be 43 for backslash based on standard evdev)

    // --- QWERTY row (keys 16-25) ---
    {16, 'Q'}, {17, 'W'}, {18, 'E'}, {19, 'R'}, {20, 'T'},
    {21, 'Y'}, {22, 'U'}, {23, 'I'}, {24, 'O'}, {25, 'P'},

    // --- ASDF row (keys 30-38) ---
    {30, 'A'}, {31, 'S'}, {32, 'D'}, {33, 'F'}, {34, 'G'},
    {35, 'H'}, {36, 'J'}, {37, 'K'}, {38, 'L'},

    // --- ZXCV row (keys 44-50) ---
    {44, 'Z'}, {45, 'X'}, {46, 'C'}, {47, 'V'}, {48, 'B'},
    {49, 'N'}, {50, 'M'},

    // --- Right-side punctuation and special ---
    {39, VK_OEM_1},      // ;
    {40, VK_OEM_7},      // '
    {41, VK_OEM_3},      // `
    {51, VK_OEM_COMMA},  // ,
    {52, VK_OEM_PERIOD}, // .
    {53, VK_OEM_2},      // /
    {41, VK_OEM_3},      // ` (tilde/Grave)

    // --- Modifiers ---
    {29, VK_CONTROL},    // Left Ctrl
    {42, VK_LSHIFT},     // Left Shift
    {54, VK_RSHIFT},     // Right Shift
    {56, VK_LMENU},      // Left Alt
    {100, VK_RMENU},     // Right Alt (AltGr)
    {97, VK_RCONTROL},   // Right Ctrl

    // --- Backspace, Tab, Space, Enter ---
    {14, VK_BACK},       // Backspace
    {15, VK_TAB},        // Tab
    {57, VK_SPACE},      // Space

    // --- Caps Lock and system keys ---
    {58, VK_CAPITAL},    // Caps Lock
    {69, VK_NUMLOCK},    // Num Lock
    {70, VK_SCROLL},     // Scroll Lock

    // --- Navigation keys (arrow keys, home/end, etc) ---
    {72, VK_UP},         // Up Arrow
    {80, VK_DOWN},       // Down Arrow
    {75, VK_LEFT},       // Left Arrow
    {77, VK_RIGHT},      // Right Arrow
    {71, VK_HOME},       // Home
    {79, VK_END},        // End
    {73, VK_PRIOR},      // Page Up
    {81, VK_NEXT},       // Page Down
    {82, VK_INSERT},     // Insert
    {111, VK_DELETE},    // Delete

    // --- Numpad (most keys overlap arrow/nav, but these distinct ones work) ---
    {96, VK_RETURN},     // Numpad Enter
    {98, VK_SEPARATOR},  // Numpad Separator
    {55, VK_MULTIPLY},   // Numpad Multiply
    {74, VK_SUBTRACT},   // Numpad Subtract
    {78, VK_ADD},        // Numpad Add

    // --- Windows and Application keys ---
    {125, VK_LWIN},      // Left Windows key
    {126, VK_RWIN},      // Right Windows key
    {127, VK_APPS},      // Application/Menu key

    // --- Special/Multimedia keys ---
    {119, VK_PAUSE},     // Pause/Break
    {120, VK_SNAPSHOT},  // Print Screen
    {122, VK_HELP},      // Help
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
    // Uncomment to debug unmapped keys:
    // printf("Warning: Unmapped Linux key code: %d\n", linux_code);
    return 0;
}
