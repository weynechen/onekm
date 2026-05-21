#ifndef HOTPLUG_H
#define HOTPLUG_H

typedef void (*hotplug_add_cb)(const char *devpath);
typedef void (*hotplug_remove_cb)(const char *devpath);

/* Start udev monitor for input device add/remove events.
 * Returns 0 on success, -1 on failure. */
int hotplug_init(hotplug_add_cb on_add, hotplug_remove_cb on_remove);

/* Returns the udev monitor fd — add to epoll with EPOLLIN. */
int hotplug_get_fd(void);

/* Call when epoll reports the udev fd is readable. */
void hotplug_process(void);

void hotplug_cleanup(void);

#endif // HOTPLUG_H
