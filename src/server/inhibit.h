#ifndef INHIBIT_H
#define INHIBIT_H

/* Initialize X11 connection for screensaver inhibition.
 * Returns 0 on success, -1 on failure (non-fatal). */
int  inhibit_init(void);

/* Call periodically to reset the X11 screensaver idle timer. */
void inhibit_reset(void);

void inhibit_cleanup(void);

#endif // INHIBIT_H
