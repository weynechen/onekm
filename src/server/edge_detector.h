#ifndef EDGE_DETECTOR_H
#define EDGE_DETECTOR_H

#include <stdint.h>

int init_edge_detector(void);
int is_at_edge(int *dx, int *dy);
void update_mouse_position(int dx, int dy);
void cleanup_edge_detector(void);

#endif // EDGE_DETECTOR_H