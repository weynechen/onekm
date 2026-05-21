#include "uinput_inject.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>

static int ufd = -1;

int uinput_inject_init(void) {
    ufd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (ufd < 0) {
        perror("[UINPUT] Failed to open /dev/uinput");
        return -1;
    }

    /* Enable event types */
    ioctl(ufd, UI_SET_EVBIT, EV_KEY);
    ioctl(ufd, UI_SET_EVBIT, EV_REL);
    ioctl(ufd, UI_SET_EVBIT, EV_MSC);
    ioctl(ufd, UI_SET_EVBIT, EV_SYN);

    /* Enable all key codes (covers full keyboard + mouse buttons) */
    for (int i = 0; i < KEY_MAX; i++) {
        ioctl(ufd, UI_SET_KEYBIT, i);
    }

    /* Enable MSC_SCAN for proper scan code passthrough */
    ioctl(ufd, UI_SET_MSCBIT, MSC_SCAN);

    /* Enable relative axes for mouse */
    ioctl(ufd, UI_SET_RELBIT, REL_X);
    ioctl(ufd, UI_SET_RELBIT, REL_Y);
    ioctl(ufd, UI_SET_RELBIT, REL_WHEEL);
    ioctl(ufd, UI_SET_RELBIT, REL_HWHEEL);

    struct uinput_setup usetup;
    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_VIRTUAL;
    usetup.id.vendor  = 0x1d6b;  /* Linux Foundation */
    usetup.id.product = 0x0001;
    usetup.id.version = 1;
    strncpy(usetup.name, "OneKM Virtual Input", UINPUT_MAX_NAME_SIZE - 1);

    if (ioctl(ufd, UI_DEV_SETUP, &usetup) < 0) {
        perror("[UINPUT] UI_DEV_SETUP failed");
        close(ufd);
        ufd = -1;
        return -1;
    }

    if (ioctl(ufd, UI_DEV_CREATE) < 0) {
        perror("[UINPUT] UI_DEV_CREATE failed");
        close(ufd);
        ufd = -1;
        return -1;
    }

    /* Allow udev time to process the new device so input_capture won't grab it */
    usleep(200000);

    printf("[UINPUT] Virtual input device created\n");
    return 0;
}

void uinput_inject_event(uint16_t type, uint16_t code, int32_t value) {
    if (ufd < 0) return;

    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type  = type;
    ev.code  = code;
    ev.value = value;

    if (write(ufd, &ev, sizeof(ev)) < 0 && errno != EAGAIN) {
        fprintf(stderr, "[UINPUT] Write error: %s\n", strerror(errno));
    }
}

void uinput_inject_cleanup(void) {
    if (ufd >= 0) {
        ioctl(ufd, UI_DEV_DESTROY);
        close(ufd);
        ufd = -1;
        printf("[UINPUT] Virtual input device destroyed\n");
    }
}
