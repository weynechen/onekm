#include "input_capture.h"
#include "state_machine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <libevdev/libevdev.h>

#define MAX_DEVICES 10
static struct libevdev *devices[MAX_DEVICES];
static int num_devices = 0;
static int grab_devices = 0;

int init_input_capture(void) {
    const char *input_dir = "/dev/input";
    DIR *dir;
    struct dirent *entry;
    int fd;

    dir = opendir(input_dir);
    if (!dir) {
        perror("Failed to open input directory");
        return -1;
    }

    printf("Scanning input devices...\n");

    while ((entry = readdir(dir)) != NULL && num_devices < MAX_DEVICES) {
        if (strncmp(entry->d_name, "event", 5) != 0) {
            continue;
        }

        char device_path[512];
        snprintf(device_path, sizeof(device_path), "%s/%s", input_dir, entry->d_name);

        fd = open(device_path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            continue;
        }

        struct libevdev *dev = NULL;
        int rc = libevdev_new_from_fd(fd, &dev);
        if (rc < 0) {
            close(fd);
            continue;
        }

        // Only add keyboard and mouse devices
        if (libevdev_has_event_type(dev, EV_KEY) ||
            libevdev_has_event_type(dev, EV_REL)) {
            devices[num_devices++] = dev;
            printf("Added device: %s (%s)\n",
                   libevdev_get_name(dev), device_path);
        } else {
            libevdev_free(dev);
            close(fd);
        }
    }

    closedir(dir);

    if (num_devices == 0) {
        fprintf(stderr, "No input devices found\n");
        return -1;
    }

    printf("Initialized %d input device(s)\n", num_devices);
    return 0;
}

void set_device_grab(int grab) {
    if (grab_devices == grab) {
        return; // Already in the desired state
    }

    grab_devices = grab;
    for (int i = 0; i < num_devices; i++) {
        if (grab) {
            libevdev_grab(devices[i], LIBEVDEV_GRAB);
        } else {
            libevdev_grab(devices[i], LIBEVDEV_UNGRAB);
        }
    }

    if (grab) {
        printf("Input devices grabbed - events will not affect local system\n");
    } else {
        printf("Input devices ungrabbed - events will affect local system\n");
    }
}

int capture_input(InputEvent *event) {
    int rc;
    struct input_event ev;

    for (int i = 0; i < num_devices; i++) {
        rc = libevdev_next_event(devices[i], LIBEVDEV_READ_FLAG_NORMAL, &ev);

        if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
            // Filter out SYN_REPORT events
            if (ev.type == EV_SYN) {
                continue;
            }

            event->type = ev.type;
            event->code = ev.code;
            event->value = ev.value;
            return 0;
        } else if (rc == -EAGAIN) {
            // No events available on this device
            continue;
        } else if (rc < 0) {
            // Error reading from device
            if (rc == -ENODEV) {
                // Device was removed
                printf("Device removed\n");
            }
            continue;
        }
    }

    return -1; // No events available
}

void cleanup_input_capture(void) {
    // Ensure devices are ungrabbed before cleanup
    set_device_grab(0);

    for (int i = 0; i < num_devices; i++) {
        if (devices[i]) {
            int fd = libevdev_get_fd(devices[i]);
            libevdev_free(devices[i]);
            close(fd);
            devices[i] = NULL;
        }
    }
    num_devices = 0;
}