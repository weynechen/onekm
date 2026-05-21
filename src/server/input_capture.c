#include "input_capture.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <linux/input.h>
#include <libevdev/libevdev.h>

#define MAX_DEVICES 16

typedef struct {
    struct libevdev *dev;
    char path[256];
} Device;

static Device devices[MAX_DEVICES];
static int    num_devices = 0;

static int path_exists(const char *path) {
    for (int i = 0; i < num_devices; i++) {
        if (strcmp(devices[i].path, path) == 0) return 1;
    }
    return 0;
}

static void remove_at(int idx) {
    int fd = libevdev_get_fd(devices[idx].dev);
    libevdev_grab(devices[idx].dev, LIBEVDEV_UNGRAB);
    libevdev_free(devices[idx].dev);
    close(fd);
    for (int j = idx; j < num_devices - 1; j++) {
        devices[j] = devices[j + 1];
    }
    num_devices--;
    memset(&devices[num_devices], 0, sizeof(devices[0]));
}

int input_capture_add_device(const char *path) {
    if (num_devices >= MAX_DEVICES) return -1;
    if (path_exists(path)) return -1;

    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) return -1;

    struct libevdev *dev = NULL;
    if (libevdev_new_from_fd(fd, &dev) < 0) {
        close(fd);
        return -1;
    }

    /* Must have keyboard or mouse events */
    if (!libevdev_has_event_type(dev, EV_KEY) &&
        !libevdev_has_event_type(dev, EV_REL)) {
        libevdev_free(dev);
        close(fd);
        return -1;
    }

    /* Skip our own uinput passthrough device */
    if (strstr(libevdev_get_name(dev), "OneKM")) {
        libevdev_free(dev);
        close(fd);
        return -1;
    }

    if (libevdev_grab(dev, LIBEVDEV_GRAB) < 0) {
        fprintf(stderr, "[INPUT] Failed to grab %s: %s\n", path, strerror(errno));
        libevdev_free(dev);
        close(fd);
        return -1;
    }

    devices[num_devices].dev = dev;
    strncpy(devices[num_devices].path, path, sizeof(devices[0].path) - 1);
    num_devices++;

    printf("[INPUT] Grabbed: %s (%s)\n", libevdev_get_name(dev), path);
    return fd;
}

void input_capture_remove_device(const char *path) {
    for (int i = 0; i < num_devices; i++) {
        if (strcmp(devices[i].path, path) == 0) {
            printf("[INPUT] Removing: %s\n", path);
            remove_at(i);
            return;
        }
    }
}

int input_capture_init(void) {
    const char *input_dir = "/dev/input";
    DIR *dir = opendir(input_dir);
    if (!dir) {
        perror("[INPUT] Failed to open /dev/input");
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) != 0) continue;
        char path[320];
        snprintf(path, sizeof(path), "%s/%s", input_dir, entry->d_name);
        input_capture_add_device(path);
    }
    closedir(dir);

    if (num_devices == 0) {
        fprintf(stderr, "[INPUT] No input devices found\n");
        return -1;
    }

    printf("[INPUT] Grabbed %d device(s)\n", num_devices);
    return 0;
}

int input_capture_get_fds(int *fds, int max_fds) {
    int count = (num_devices < max_fds) ? num_devices : max_fds;
    for (int i = 0; i < count; i++) {
        fds[i] = libevdev_get_fd(devices[i].dev);
    }
    return count;
}

int input_capture_read_fd(int fd, InputEvent *event) {
    for (int i = 0; i < num_devices; i++) {
        if (libevdev_get_fd(devices[i].dev) != fd) continue;

        struct input_event ev;
        int rc;

        rc = libevdev_next_event(devices[i].dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);

        if (rc == LIBEVDEV_READ_STATUS_SYNC) {
            /* Drain dropped-event sync queue silently */
            while (rc == LIBEVDEV_READ_STATUS_SYNC) {
                rc = libevdev_next_event(devices[i].dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
            }
            return -1;
        }

        if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
            event->type  = ev.type;
            event->code  = ev.code;
            event->value = ev.value;
            return 0;
        }

        /* rc < 0 */
        if (rc == -ENODEV) {
            printf("[INPUT] Device disconnected: %s\n", devices[i].path);
            remove_at(i);
        }
        return -1;
    }
    return -1;
}

void input_capture_cleanup(void) {
    while (num_devices > 0) {
        remove_at(0);
    }
}
