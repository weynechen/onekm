#include "state_machine.h"
#include <stdio.h>

static ControlState current      = STATE_LOCAL;
static int          exit_request = 0;

void state_init(void) {
    current      = STATE_LOCAL;
    exit_request = 0;
    printf("[STATE] Initialized in LOCAL mode\n");
    printf("[STATE] Press PAUSE to toggle LOCAL/REMOTE (press 3x within 2s to exit)\n");
}

ControlState state_get(void) {
    return current;
}

void state_set(ControlState s) {
    static const char *names[] = { "LOCAL", "REMOTE" };
    printf("[STATE] %s -> %s\n", names[current], names[s]);
    current = s;
}

int state_should_exit(void) {
    return exit_request;
}

void state_request_exit(void) {
    exit_request = 1;
}
