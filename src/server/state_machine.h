#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

typedef enum {
    STATE_LOCAL,
    STATE_REMOTE
} ControlState;

void         state_init(void);
ControlState state_get(void);
void         state_set(ControlState s);
int          state_should_exit(void);
void         state_request_exit(void);

#endif // STATE_MACHINE_H
