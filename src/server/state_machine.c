#include "state_machine.h"
#include "edge_detector.h"
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

    // Handle edge detection for switching
    if (current_state == STATE_LOCAL) {
        int edge_dx, edge_dy;
        if (event->type == EV_REL && event->code == REL_X) {
            update_mouse_position(event->value, 0);
        } else if (event->type == EV_REL && event->code == REL_Y) {
            update_mouse_position(0, event->value);
        }

        if (is_at_edge(&edge_dx, &edge_dy)) {
            // Switch to remote control
            current_state = STATE_REMOTE;
            msg_switch(msg, 1); // 1 = switch to remote
            printf("Switched to REMOTE control (edge: dx=%d, dy=%d)\n", edge_dx, edge_dy);
            return 1;
        }
    }

    // Process events based on current state
    switch (current_state) {
        case STATE_LOCAL:
            // Don't send events when in local control
            return 0;

        case STATE_REMOTE:
            // Send all input events to remote client
            if (event->type == EV_REL) {
                if (event->code == REL_X || event->code == REL_Y) {
                    static int dx = 0, dy = 0;

                    if (event->code == REL_X) {
                        dx = event->value;
                        if (dx != 0) {
                            msg_mouse_move(msg, dx, dy);
                            dy = 0; // Reset dy after sending
                            return 1;
                        }
                    } else if (event->code == REL_Y) {
                        dy = event->value;
                        if (dy != 0) {
                            msg_mouse_move(msg, dx, dy);
                            dx = 0; // Reset dx after sending
                            return 1;
                        }
                    }
                }
            } else if (event->type == EV_KEY) {
                // Map Linux key codes to our protocol
                // Left mouse button
                if (event->code == BTN_LEFT) {
                    msg_mouse_button(msg, 1, event->value);
                    return 1;
                }
                // Right mouse button
                else if (event->code == BTN_RIGHT) {
                    msg_mouse_button(msg, 2, event->value);
                    return 1;
                }
                // Middle mouse button
                else if (event->code == BTN_MIDDLE) {
                    msg_mouse_button(msg, 3, event->value);
                    return 1;
                }
                // Keyboard events
                else if (event->code < 256) {
                    msg_key_event(msg, event->code, event->value);
                    return 1;
                }
            }

            // Check for escape key to return control
            if (event->type == EV_KEY && event->code == KEY_ESC && event->value == 1) {
                current_state = STATE_LOCAL;
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
    printf("State machine cleaned up\n");
}