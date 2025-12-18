#include "state_machine.h"
#include "edge_detector.h"
#include "input_capture.h"
#include <stdio.h>
#include <linux/input.h>

static ControlState current_state = STATE_LOCAL;

void init_state_machine(void) {
    current_state = STATE_LOCAL;
    printf("State machine initialized in LOCAL mode\n");
}

int process_event(const InputEvent *event, Message *msg) {
    if (!event || !msg) {
        return 0;
    }

    // Track mouse position for edge detection in both states
    if (event->type == EV_REL && event->code == REL_X) {
        update_mouse_position(event->value, 0);
    } else if (event->type == EV_REL && event->code == REL_Y) {
        update_mouse_position(0, event->value);
    }

    // Handle edge detection for switching
    int edge_dx, edge_dy;
    if (is_at_edge(&edge_dx, &edge_dy)) {
        if (current_state == STATE_LOCAL && edge_dx < 0) {
            // Switch to remote control when hitting left edge
            current_state = STATE_REMOTE;
            set_device_grab(1); // Grab devices so input doesn't affect local system
            msg_switch(msg, 1); // 1 = switch to remote
            start_remote_mode(); // Reset position for remote mode
            printf("Switched to REMOTE control (edge: dx=%d, dy=%d)\n", edge_dx, edge_dy);
            return 1;
        } else if (current_state == STATE_REMOTE) {
            // Switch to local control when hitting ANY edge while in remote state
            // This allows returning to local from any edge (left, right, top, bottom)
            current_state = STATE_LOCAL;
            set_device_grab(0); // Ungrab devices so input affects local system again
            msg_switch(msg, 0); // 0 = switch to local
            end_remote_mode(); // Restore normal edge detection
            printf("Switched to LOCAL control (edge: dx=%d, dy=%d)\n", edge_dx, edge_dy);
            return 1;
        }
    }

    // Process events based on current state
    switch (current_state) {
        case STATE_LOCAL:
            // Don't send events when in local control
            return 0;

        case STATE_REMOTE:
            // Send mouse movements to remote client (but not the edge-crossing ones)
            if (event->type == EV_REL) {
                if (event->code == REL_X || event->code == REL_Y) {
                    static int pending_dx = 0, pending_dy = 0;

                    if (event->code == REL_X) {
                        pending_dx = event->value;
                        if (pending_dx != 0) {
                            msg_mouse_move(msg, pending_dx, pending_dy);
                            pending_dy = 0; // Reset dy after sending
                            return 1;
                        }
                    } else if (event->code == REL_Y) {
                        pending_dy = event->value;
                        if (pending_dy != 0) {
                            msg_mouse_move(msg, pending_dx, pending_dy);
                            pending_dx = 0; // Reset dx after sending
                            return 1;
                        }
                    }
                }
            } else if (event->type == EV_KEY) {
                // Map Linux key codes to our protocol
                // Left mouse button
                if (event->code == BTN_LEFT) {
                    printf("Server: BTN_LEFT %s\n", event->value ? "DOWN" : "UP");
                    msg_mouse_button(msg, 1, event->value);
                    return 1;
                }
                // Right mouse button
                else if (event->code == BTN_RIGHT) {
                    printf("Server: BTN_RIGHT %s\n", event->value ? "DOWN" : "UP");
                    msg_mouse_button(msg, 2, event->value);
                    return 1;
                }
                // Middle mouse button
                else if (event->code == BTN_MIDDLE) {
                    printf("Server: BTN_MIDDLE %s\n", event->value ? "DOWN" : "UP");
                    msg_mouse_button(msg, 3, event->value);
                    return 1;
                }
                // Keyboard events
                else if (event->code < 256) {
                    msg_key_event(msg, event->code, event->value);
                    return 1;
                }
                // Debug unknown buttons
                else {
                    printf("Server: Unknown EV_KEY code=%d value=%d\n", event->code, event->value);
                }
            }

            // Check for escape key to return control
            if (event->type == EV_KEY && event->code == KEY_ESC && event->value == 1) {
                current_state = STATE_LOCAL;
                set_device_grab(0); // Ungrab devices so input affects local system again
                msg_switch(msg, 0); // 0 = switch to local
                printf("Switched to LOCAL control (ESC key)\n");
                return 1;
            }
            break;
    }

    return 0;
}

void cleanup_state_machine(void) {
    current_state = STATE_LOCAL;
    set_device_grab(0); // Ensure devices are ungrabbed on cleanup
    printf("State machine cleaned up\n");
}