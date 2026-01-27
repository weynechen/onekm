#include "key_sync.h"
#include "input_capture.h"
#include "keyboard_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <linux/input.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

static Display *x_display = NULL;
static int xtest_available = 0;

// X11 keycode to Linux evdev keycode offset
// X11 keycodes are typically evdev keycodes + 8
#define X11_KEYCODE_OFFSET 8

int key_sync_init(void) {
    // Open X11 display for querying keyboard state and injecting events
    x_display = XOpenDisplay(NULL);
    if (!x_display) {
        fprintf(stderr, "Warning: Failed to open X11 display for key sync\n");
        fprintf(stderr, "Key synchronization will be disabled\n");
        return -1;
    }
    
    printf("X11 display opened for key synchronization\n");

    // Check if XTest extension is available
    int event_base, error_base, major_version, minor_version;
    if (XTestQueryExtension(x_display, &event_base, &error_base, 
                            &major_version, &minor_version)) {
        xtest_available = 1;
        printf("XTest extension available (version %d.%d)\n", 
               major_version, minor_version);
    } else {
        fprintf(stderr, "Warning: XTest extension not available\n");
        fprintf(stderr, "Key synchronization may not work properly\n");
        xtest_available = 0;
    }

    printf("Key sync module initialized\n");
    return 0;
}

// Inject a key release event using XTest
static int inject_key_release(int x11_keycode) {
    if (!x_display || !xtest_available) {
        return -1;
    }
    
    // XTestFakeKeyEvent: display, keycode, is_press, delay
    // is_press = False means key release
    if (!XTestFakeKeyEvent(x_display, x11_keycode, False, CurrentTime)) {
        fprintf(stderr, "Failed to inject key release for X11 keycode %d\n", x11_keycode);
        return -1;
    }
    
    printf("[KEY_SYNC] Injected release for X11 keycode %d (linux %d)\n", 
           x11_keycode, x11_keycode - X11_KEYCODE_OFFSET);
    return 0;
}

int key_sync_on_mode_switch(void) {
    if (!x_display) {
        fprintf(stderr, "Key sync module not initialized\n");
        return -1;
    }

    // Get hardware keyboard state (what keys are physically pressed)
    uint8_t hw_key_states[32] = {0};
    if (get_hardware_keyboard_state(hw_key_states) < 0) {
        fprintf(stderr, "Failed to get hardware keyboard state\n");
        return -1;
    }

    int keys_synced = 0;

    // Query X11 server's keyboard state
    char x11_key_states[32] = {0};
    XQueryKeymap(x_display, x11_key_states);

    // Compare X11 state with hardware state
    // X11 uses keycodes starting from 8 (evdev keycode + 8)
    for (int x11_keycode = 8; x11_keycode < 256; x11_keycode++) {
        int linux_keycode = x11_keycode - X11_KEYCODE_OFFSET;
        if (linux_keycode < 0 || linux_keycode >= 256) {
            continue;
        }

        // Check if X11 thinks this key is pressed
        int x11_pressed = (x11_key_states[x11_keycode / 8] >> (x11_keycode % 8)) & 1;
        
        // Check if hardware reports this key as pressed
        int hw_pressed = (hw_key_states[linux_keycode / 8] >> (linux_keycode % 8)) & 1;

        // If X11 thinks key is pressed but hardware says it's not,
        // we need to inject a release event
        if (x11_pressed && !hw_pressed) {
            printf("[KEY_SYNC] X11 stuck key detected: linux_keycode=%d (0x%02X), x11_keycode=%d\n", 
                   linux_keycode, linux_keycode, x11_keycode);
            
            // Inject key release event using XTest
            if (inject_key_release(x11_keycode) == 0) {
                keys_synced++;
            }
        }
    }

    if (keys_synced > 0) {
        printf("[KEY_SYNC] Synchronized %d key(s)\n", keys_synced);
        
        // Flush and sync X11 to ensure our injected events are processed
        XFlush(x_display);
        XSync(x_display, False);
        
        // Small delay to ensure events are fully processed
        usleep(10000); // 10ms
    } else {
        printf("[KEY_SYNC] No keys needed synchronization\n");
    }

    return keys_synced;
}

void key_sync_cleanup(void) {
    if (x_display) {
        XCloseDisplay(x_display);
        x_display = NULL;
        xtest_available = 0;
        printf("Key sync module cleaned up (X11 display closed)\n");
    }
}
