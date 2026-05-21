#include "inhibit.h"
#include <stdio.h>
#include <X11/Xlib.h>

static Display *display = NULL;

int inhibit_init(void) {
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "[INHIBIT] Failed to open X11 display — screensaver will not be inhibited\n");
        return -1;
    }
    printf("[INHIBIT] X11 screensaver inhibit ready\n");
    return 0;
}

void inhibit_reset(void) {
    if (!display) return;
    XResetScreenSaver(display);
    XFlush(display);
}

void inhibit_cleanup(void) {
    if (display) {
        XCloseDisplay(display);
        display = NULL;
    }
}
