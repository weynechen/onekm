#include "hotplug.h"
#include <stdio.h>

#ifdef HAVE_LIBUDEV
#include <string.h>
#include <libudev.h>

static struct udev         *udev_ctx = NULL;
static struct udev_monitor *udev_mon = NULL;
static hotplug_add_cb    on_add    = NULL;
static hotplug_remove_cb on_remove = NULL;

int hotplug_init(hotplug_add_cb add_cb, hotplug_remove_cb remove_cb) {
    on_add    = add_cb;
    on_remove = remove_cb;

    udev_ctx = udev_new();
    if (!udev_ctx) {
        fprintf(stderr, "[HOTPLUG] Failed to create udev context\n");
        return -1;
    }

    udev_mon = udev_monitor_new_from_netlink(udev_ctx, "udev");
    if (!udev_mon) {
        fprintf(stderr, "[HOTPLUG] Failed to create udev monitor\n");
        udev_unref(udev_ctx);
        udev_ctx = NULL;
        return -1;
    }

    udev_monitor_filter_add_match_subsystem_devtype(udev_mon, "input", NULL);
    udev_monitor_enable_receiving(udev_mon);

    printf("[HOTPLUG] udev monitor ready\n");
    return 0;
}

int hotplug_get_fd(void) {
    return udev_mon ? udev_monitor_get_fd(udev_mon) : -1;
}

void hotplug_process(void) {
    if (!udev_mon) return;

    struct udev_device *dev = udev_monitor_receive_device(udev_mon);
    if (!dev) return;

    const char *action  = udev_device_get_action(dev);
    const char *devnode = udev_device_get_devnode(dev);

    if (action && devnode && strncmp(devnode, "/dev/input/event", 16) == 0) {
        if (strcmp(action, "add") == 0 && on_add) {
            on_add(devnode);
        } else if (strcmp(action, "remove") == 0 && on_remove) {
            on_remove(devnode);
        }
    }

    udev_device_unref(dev);
}

void hotplug_cleanup(void) {
    if (udev_mon) { udev_monitor_unref(udev_mon); udev_mon = NULL; }
    if (udev_ctx) { udev_unref(udev_ctx);         udev_ctx = NULL; }
}

#else /* HAVE_LIBUDEV not defined — stub implementation */

int hotplug_init(hotplug_add_cb add_cb, hotplug_remove_cb remove_cb) {
    (void)add_cb;
    (void)remove_cb;
    printf("[HOTPLUG] Disabled (libudev not available — install libudev-dev for hotplug support)\n");
    return -1;
}
int  hotplug_get_fd(void)   { return -1; }
void hotplug_process(void)  {}
void hotplug_cleanup(void)  {}

#endif /* HAVE_LIBUDEV */
