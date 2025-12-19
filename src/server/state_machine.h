#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "input_capture.h"
#include "common/protocol.h"

typedef enum {
    STATE_LOCAL,
    STATE_REMOTE
} ControlState;

void init_state_machine(void);
int process_event(const InputEvent *event, Message *msg);
int flush_pending_mouse_movement(Message *msg);
void cleanup_state_machine(void);
ControlState get_current_state(void);

#endif // STATE_MACHINE_H