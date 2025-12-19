#include "state_machine.h"
#include "input_capture.h"
#include "common/protocol.h"
#include <stdio.h>
#include <linux/input.h>

static ControlState current_state = STATE_LOCAL;

// Key codes for toggle - using F12 to avoid Alt key issues
#define KEY_F12 88

void init_state_machine(void) {
    current_state = STATE_LOCAL;
    printf("State machine initialized in LOCAL mode\n");
    printf("Press F12 to toggle between LOCAL and REMOTE control\n");
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

    // Handle F12 as toggle - 处理按下和释放两种事件
    if (event->type == EV_KEY && event->code == KEY_F12) {
        // 只在按下时处理（value == 1）
        if (event->value == 1) {
            // 在切换模式前发送待处理的鼠标移动
            // int sent = send_pending_movement(msg);

            printf("F12 pressed, current_state=%d\n", current_state);
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
        // F12键的所有事件（包括释放）都返回1，表示已处理，不再传递
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
                // 返回0，键盘事件在main.c中通过keyboard_state_process_key处理
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
