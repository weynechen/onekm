#include "state_machine.h"
#include "input_capture.h"
#include "common/protocol.h"
#include <stdio.h>
#include <linux/input.h>

static ControlState current_state = STATE_LOCAL;
static int exit_requested = 0;
static int ctrl_pressed = 0;

// Note: KEY_PAUSE is used for mode switching - defined in linux/input.h as 119

// Linux input event codes for mouse wheel
#define REL_WHEEL       0x08
#define REL_HWHEEL      0x06

void init_state_machine(void) {
    current_state = STATE_LOCAL;
    printf("State machine initialized in LOCAL mode\n");
    printf("Press PAUSE/Break to toggle between LOCAL and REMOTE control\n");
}

// 静态变量用于鼠标移动的累积
static int pending_dx = 0;
static int pending_dy = 0;
static int last_event_type = -1;

static int send_pending_movement(Message *msg) {
    if (pending_dx != 0 || pending_dy != 0) {
        // Clamp values to int16_t range to prevent overflow
        int16_t dx = (int16_t)(pending_dx > 32767 ? 32767 : (pending_dx < -32768 ? -32768 : pending_dx));
        int16_t dy = (int16_t)(pending_dy > 32767 ? 32767 : (pending_dy < -32768 ? -32768 : pending_dy));

        msg_mouse_move(msg, dx, dy);

        // Subtract the values we sent (for remaining that didn't fit)
        pending_dx -= dx;
        pending_dy -= dy;

        if (pending_dx == 0 && pending_dy == 0) {
            last_event_type = -1;
        }
        return 1;
    }
    return 0;
}

int flush_pending_mouse_movement(Message *msg) {
    return send_pending_movement(msg);
}

int process_event(const InputEvent *event, Message *msg) {
    if (!event || !msg) {
        return 0;
    }

    // Track Ctrl key state for Ctrl+C detection in LOCAL mode
    if (event->type == EV_KEY) {
        if (event->code == KEY_LEFTCTRL || event->code == KEY_RIGHTCTRL) {
            ctrl_pressed = (event->value == 1 || event->value == 2); // 1=press, 2=repeat
        }
        // Check for Ctrl+C combination in LOCAL mode
        if (current_state == STATE_LOCAL && event->code == KEY_C && event->value == 1 && ctrl_pressed) {
            printf("Ctrl+C detected in LOCAL mode - requesting exit\n");
            exit_requested = 1;
            return 0;
        }
    }

    // Handle PAUSE/Break as toggle - 处理按下和释放两种事件
    if (event->type == EV_KEY && event->code == KEY_PAUSE) {
        // 只在按下时处理（value == 1）
        if (event->value == 1) {
            // 在切换模式前发送待处理的鼠标移动
            // int sent = send_pending_movement(msg);

            printf("PAUSE pressed, current_state=%d\n", current_state);
            if (current_state == STATE_LOCAL) {
                // Switch to remote control
                current_state = STATE_REMOTE;
                set_device_grab(1); // Grab devices so input doesn't affect local system
                msg_switch(msg, 1); // 1 = switch to remote
                printf("Switching to REMOTE control, sending SWITCH message\n");
                return 1; // Always return 1 to indicate message was prepared
            } else {
                // Switch to local control
                current_state = STATE_LOCAL;
                set_device_grab(0); // Ungrab devices so input affects local system again
                msg_switch(msg, 0); // 0 = switch to local
                printf("Switching to LOCAL control, sending SWITCH message\n");
                return 1; // Always return 1 to indicate message was prepared
            }
        }
        // PAUSE键的所有事件（包括释放）都返回1，表示已处理，不再传递
        return 1;
    }

    // Process events based on current state
    switch (current_state) {
        case STATE_LOCAL:
            // Don't send events when in local control
            return 0;

        case STATE_REMOTE:
            // Send events to remote client
            if (event->type == EV_REL) {
                if (event->code == REL_X || event->code == REL_Y) {
                    // Accumulate mouse movement
                    if (event->code == REL_X) {
                        pending_dx += event->value;
                    } else if (event->code == REL_Y) {
                        pending_dy += event->value;
                    }

                    // Send immediately if we have movement in both axes
                    // or if the same axis event occurs twice in a row (rapid movement)
                    if ((pending_dx != 0 && pending_dy != 0) ||
                        (last_event_type == event->code && (pending_dx != 0 || pending_dy != 0))) {
                        int result = send_pending_movement(msg);
                        return result;
                    }

                    last_event_type = event->code;
                } else if (event->code == REL_WHEEL || event->code == REL_HWHEEL) {
                    // Mouse wheel events - send immediately
                    int16_t vertical = 0;
                    int16_t horizontal = 0;

                    if (event->code == REL_WHEEL) {
                        vertical = event->value; // Use the same direction as Linux input
                        printf("[WHEEL] Vertical scroll detected: raw=%d, converted=%d\n", event->value, vertical);
                    } else {
                        horizontal = event->value;
                        printf("[WHEEL] Horizontal scroll detected: value=%d\n", horizontal);
                    }

                    // Send any pending mouse movement first
                    if (pending_dx != 0 || pending_dy != 0) {
                        printf("[WHEEL] Has pending mouse movement (dx=%d, dy=%d), will send separately\n", pending_dx, pending_dy);
                        // Note: We can't send both in one call, so just send wheel event
                        // The pending movement will be sent on next flush
                    }
                    
                    // Send wheel event
                    msg_mouse_wheel(msg, vertical, horizontal);
                    printf("[WHEEL] Sent wheel event: vertical=%d, horizontal=%d\n", vertical, horizontal);
                    return 1;
                }
            } else if (event->type == EV_KEY) {
                // For non-movement events, send any pending mouse movement first
                if (pending_dx != 0 || pending_dy != 0) {
                    send_pending_movement(msg);
                }
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
                // Return 0, keyboard events are handled via keyboard_state_process_key in main.c
                return 0;
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

ControlState get_current_state(void) {
    return current_state;
}

int should_exit(void) {
    return exit_requested;
}
