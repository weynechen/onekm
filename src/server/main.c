#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/epoll.h>
#include <linux/input.h>

#include "common/protocol.h"
#include "input_capture.h"
#include "uinput_inject.h"
#include "hotplug.h"
#include "inhibit.h"
#include "uart.h"
#include "state_machine.h"
#include "keyboard_state.h"

/* ------------------------------------------------------------------ */
/* Constants                                                            */
/* ------------------------------------------------------------------ */
#define MAX_DEVICES       16
#define MAX_EPOLL_EVENTS  32
#define HEARTBEAT_INTERVAL_S  30   /* mouse-wiggle interval to keep Windows awake */
#define INHIBIT_INTERVAL_S    25   /* XResetScreenSaver interval                  */
#define PAUSE_EXIT_COUNT       3   /* triple-press PAUSE to quit                  */
#define PAUSE_EXIT_WINDOW_S    2   /* within this many seconds                    */
#define WIN_L_HOLD_MS         50   /* delay between Win+L press and release HID   */

/* ------------------------------------------------------------------ */
/* Global state                                                         */
/* ------------------------------------------------------------------ */
static volatile int running = 1;
static int epoll_fd = -1;

/* PAUSE key press counting for exit */
static int    pause_count      = 0;
static time_t last_pause_time  = 0;

/* Win+L tracking */
static int meta_held    = 0;   /* is Left/Right Meta currently held in LOCAL mode? */
static int remote_locked = 0;  /* Win+L was sent; suspend heartbeat until REMOTE */

/* Accumulated mouse movement (REMOTE mode) */
static int pending_dx = 0;
static int pending_dy = 0;

/* Mouse button state for clean release on mode switch */
static uint8_t mouse_buttons = 0;  /* bit0=left bit1=right bit2=middle */

/* Heartbeat / inhibit timers */
static time_t last_heartbeat = 0;
static time_t last_inhibit   = 0;

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */
static void epoll_add(int fd) {
    struct epoll_event ev;
    ev.events  = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0 && errno != EEXIST) {
        fprintf(stderr, "[MAIN] epoll_ctl ADD fd=%d: %s\n", fd, strerror(errno));
    }
}

/* Note: no explicit epoll_del needed — Linux removes closed fds from epoll automatically */

/* ------------------------------------------------------------------ */
/* Remote event sending                                                 */
/* ------------------------------------------------------------------ */
static void flush_mouse(void) {
    if (pending_dx == 0 && pending_dy == 0) return;
    Message msg;
    int16_t dx = (int16_t)(pending_dx > 32767 ? 32767 : pending_dx < -32768 ? -32768 : pending_dx);
    int16_t dy = (int16_t)(pending_dy > 32767 ? 32767 : pending_dy < -32768 ? -32768 : pending_dy);
    msg_mouse_move(&msg, dx, dy);
    uart_send(&msg);
    pending_dx -= dx;
    pending_dy -= dy;
}

/* Release all held keys/buttons on the remote machine. */
static void remote_release_all(void) {
    Message msg;

    flush_mouse();

    HIDKeyboardReport zero = {0};
    msg_keyboard_report(&msg, &zero);
    uart_send(&msg);
    keyboard_state_reset(NULL);

    for (int i = 0; i < 3; i++) {
        if (mouse_buttons & (uint8_t)(1u << i)) {
            msg_mouse_button(&msg, (uint8_t)(i + 1), BUTTON_RELEASED);
            uart_send(&msg);
        }
    }
    mouse_buttons = 0;
}

static void handle_remote_key(const InputEvent *ev) {
    Message msg;

    /* Mouse buttons (BTN_LEFT=272, BTN_RIGHT=273, BTN_MIDDLE=274) */
    if (ev->code == BTN_LEFT || ev->code == BTN_RIGHT || ev->code == BTN_MIDDLE) {
        uint8_t btn = (ev->code == BTN_LEFT) ? 1u : (ev->code == BTN_RIGHT) ? 2u : 3u;
        uint8_t bit = (uint8_t)(1u << (btn - 1));
        if (ev->value) mouse_buttons |=  bit;
        else           mouse_buttons &= ~bit;
        msg_mouse_button(&msg, btn, (uint8_t)ev->value);
        uart_send(&msg);
        return;
    }

    /* Key repeat (value==2): target machine handles its own repeat */
    if (ev->value == 2) return;

    HIDKeyboardReport report;
    if (keyboard_state_process_key(ev->code, (uint8_t)ev->value, &report)) {
        msg_keyboard_report(&msg, &report);
        uart_send(&msg);
    }
}

static void handle_remote_rel(const InputEvent *ev) {
    Message msg;

    if (ev->code == REL_X) {
        pending_dx += ev->value;
    } else if (ev->code == REL_Y) {
        pending_dy += ev->value;
        /* Flush when we have both axes — reduces packets */
        if (pending_dx != 0 || pending_dy != 0) flush_mouse();
    } else if (ev->code == REL_WHEEL || ev->code == REL_HWHEEL) {
        flush_mouse();
        int16_t vert  = (ev->code == REL_WHEEL)  ? (int16_t)ev->value : 0;
        int16_t horiz = (ev->code == REL_HWHEEL) ? (int16_t)ev->value : 0;
        msg_mouse_wheel(&msg, vert, horiz);
        uart_send(&msg);
    }
}

/* ------------------------------------------------------------------ */
/* Win+L: lock both machines                                            */
/* ------------------------------------------------------------------ */
static void trigger_remote_lock(void) {
    Message msg;
    HIDKeyboardReport rpt = {0};

    /* Press Win+L */
    rpt.modifiers = MODIFIER_LEFT_GUI;
    rpt.keys[0]   = 15;  /* HID usage code for 'L' */
    msg_keyboard_report(&msg, &rpt);
    uart_send(&msg);

    /* Hold briefly so the target OS registers the combo */
    usleep(WIN_L_HOLD_MS * 1000);

    /* Release */
    memset(&rpt, 0, sizeof(rpt));
    msg_keyboard_report(&msg, &rpt);
    uart_send(&msg);

    remote_locked = 1;
    printf("[LOCK] Win+L sent to remote; heartbeat suspended until next REMOTE session\n");
}

/* ------------------------------------------------------------------ */
/* Mode switching                                                       */
/* ------------------------------------------------------------------ */
static void switch_to_remote(void) {
    keyboard_state_reset(NULL);
    pending_dx  = 0;
    pending_dy  = 0;
    mouse_buttons = 0;

    Message msg;
    msg_switch(&msg, CONTROL_REMOTE);
    uart_send(&msg);

    state_set(STATE_REMOTE);
}

static void switch_to_local(void) {
    remote_release_all();

    Message msg;
    msg_switch(&msg, CONTROL_LOCAL);
    uart_send(&msg);

    /* User is actively switching back — clear any lock suspension */
    remote_locked = 0;
    meta_held     = 0;

    state_set(STATE_LOCAL);
}

/* ------------------------------------------------------------------ */
/* PAUSE key handler                                                    */
/* ------------------------------------------------------------------ */
static void handle_pause_press(void) {
    time_t now = time(NULL);

    if (now - last_pause_time <= PAUSE_EXIT_WINDOW_S) {
        pause_count++;
    } else {
        pause_count = 1;
    }
    last_pause_time = now;

    if (pause_count >= PAUSE_EXIT_COUNT) {
        printf("[MAIN] PAUSE x%d — exiting\n", PAUSE_EXIT_COUNT);
        state_request_exit();
        running = 0;
        return;
    }

    printf("[PAUSE] %d/%d\n", pause_count, PAUSE_EXIT_COUNT);

    if (state_get() == STATE_LOCAL) {
        switch_to_remote();
    } else {
        switch_to_local();
    }
}

/* ------------------------------------------------------------------ */
/* Central event dispatcher                                             */
/* ------------------------------------------------------------------ */
static void dispatch_event(const InputEvent *ev) {
    /* PAUSE is always consumed here, never forwarded */
    if (ev->type == EV_KEY && ev->code == KEY_PAUSE) {
        if (ev->value == 1) handle_pause_press();
        return;
    }

    if (state_get() == STATE_LOCAL) {
        /* Track Meta key for Win+L detection */
        if (ev->type == EV_KEY) {
            if (ev->code == KEY_LEFTMETA || ev->code == KEY_RIGHTMETA) {
                meta_held = (ev->value != 0);
            }
            /* Win+L: inject locally (triggers Linux lock) AND lock remote */
            if (ev->code == KEY_L && ev->value == 1 && meta_held) {
                trigger_remote_lock();
                /* Fall through: still inject to uinput so Linux locks too */
            }
        }

        /* Pass the raw event to the local virtual device */
        uinput_inject_event(ev->type, ev->code, ev->value);

    } else { /* STATE_REMOTE */
        /* Ignore SYN/MSC — only process actionable input */
        if (ev->type == EV_SYN || ev->type == EV_MSC) return;

        if (ev->type == EV_KEY) {
            handle_remote_key(ev);
        } else if (ev->type == EV_REL) {
            handle_remote_rel(ev);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Hotplug callbacks                                                    */
/* ------------------------------------------------------------------ */
static void on_device_added(const char *path) {
    int fd = input_capture_add_device(path);
    if (fd >= 0) {
        epoll_add(fd);
    }
}

static void on_device_removed(const char *path) {
    /* Removal from our device list already happened in read_fd (ENODEV).
     * The closed fd is automatically removed from epoll by the kernel.
     * This callback is just informational. */
    printf("[HOTPLUG] Remove event for %s\n", path);
    input_capture_remove_device(path);
}

/* ------------------------------------------------------------------ */
/* Signal handler                                                       */
/* ------------------------------------------------------------------ */
static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

/* ------------------------------------------------------------------ */
/* Periodic tasks                                                       */
/* ------------------------------------------------------------------ */
static void handle_periodic(void) {
    time_t now = time(NULL);

    /* Prevent X11 screensaver */
    if (now - last_inhibit >= INHIBIT_INTERVAL_S) {
        inhibit_reset();
        last_inhibit = now;
    }

    /* Heartbeat: small mouse wiggle to keep Windows from sleeping.
     * Only when LOCAL (we're not actively using the remote) and not locked. */
    if (state_get() == STATE_LOCAL && !remote_locked) {
        if (last_heartbeat == 0) {
            last_heartbeat = now;
        } else if (now - last_heartbeat >= HEARTBEAT_INTERVAL_S) {
            Message msg;
            /* Two-step wiggle: +1 then -1 pixel so cursor returns to origin */
            msg_mouse_move(&msg, 1, 1);
            uart_send(&msg);
            msg_mouse_move(&msg, -1, -1);
            uart_send(&msg);
            last_heartbeat = now;
            printf("[HEARTBEAT] Sent mouse wiggle to keep remote awake\n");
        }
    }

    /* Flush any residual mouse movement in REMOTE mode */
    if (state_get() == STATE_REMOTE) {
        flush_mouse();
    }
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[]) {
    const char *uart_port = "/dev/ttyACM0";
    int baud_rate = 230400;

    if (argc > 1) uart_port = argv[1];
    if (argc > 2) {
        baud_rate = atoi(argv[2]);
        if (baud_rate != 115200 && baud_rate != 230400 &&
            baud_rate != 460800 && baud_rate != 921600) {
            fprintf(stderr, "[MAIN] Unsupported baud rate %d, defaulting to 230400\n", baud_rate);
            baud_rate = 230400;
        }
    }

    printf("OneKM Server 2.0\n");
    printf("UART: %s @ %d baud\n", uart_port, baud_rate);

    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);

    /* Initialise uinput FIRST so the virtual device exists before we scan
     * /dev/input — otherwise we might accidentally grab our own device. */
    if (uinput_inject_init() != 0) {
        fprintf(stderr, "[MAIN] Failed to create uinput virtual device\n");
        return 1;
    }

    if (input_capture_init() != 0) {
        fprintf(stderr, "[MAIN] Failed to grab input devices\n");
        uinput_inject_cleanup();
        return 1;
    }

    if (hotplug_init(on_device_added, on_device_removed) != 0) {
        fprintf(stderr, "[MAIN] Warning: hotplug unavailable\n");
    }

    inhibit_init();   /* non-fatal if X11 not available */

    if (uart_init(uart_port, baud_rate) != 0) {
        fprintf(stderr, "[MAIN] Failed to initialise UART\n");
        hotplug_cleanup();
        input_capture_cleanup();
        uinput_inject_cleanup();
        return 1;
    }

    state_init();
    keyboard_state_init();

    /* Build epoll set */
    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("[MAIN] epoll_create1");
        goto shutdown;
    }

    /* Register all input device fds */
    {
        int fds[MAX_DEVICES];
        int n = input_capture_get_fds(fds, MAX_DEVICES);
        for (int i = 0; i < n; i++) epoll_add(fds[i]);
    }

    /* Register udev monitor fd */
    {
        int ufd = hotplug_get_fd();
        if (ufd >= 0) epoll_add(ufd);
    }

    printf("[MAIN] Ready. PAUSE = toggle LOCAL/REMOTE, PAUSE x3 = exit\n");

    /* ---- Main event loop ---- */
    struct epoll_event events[MAX_EPOLL_EVENTS];

    while (running && !state_should_exit()) {
        int n = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, 200 /* ms */);

        if (n < 0) {
            if (errno == EINTR) continue;
            perror("[MAIN] epoll_wait");
            break;
        }

        int udev_fd = hotplug_get_fd();

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            if (fd == udev_fd) {
                hotplug_process();
                continue;
            }

            /* Drain all buffered events from this device fd */
            InputEvent ev;
            while (input_capture_read_fd(fd, &ev) == 0) {
                dispatch_event(&ev);
            }
        }

        handle_periodic();
    }

shutdown:
    printf("[MAIN] Shutting down...\n");

    if (state_get() == STATE_REMOTE) {
        remote_release_all();
        Message msg;
        msg_switch(&msg, CONTROL_LOCAL);
        uart_send(&msg);
    }

    if (epoll_fd >= 0) close(epoll_fd);

    uart_cleanup();
    hotplug_cleanup();
    input_capture_cleanup();
    inhibit_cleanup();
    uinput_inject_cleanup();

    printf("[MAIN] Done\n");
    return 0;
}
