#include "edge_detector.h"
#include <stdio.h>
#include <stdlib.h>

// Screen dimensions (should be configurable)
#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080
#define EDGE_THRESHOLD 10

static int mouse_x = SCREEN_WIDTH / 2;
static int mouse_y = SCREEN_HEIGHT / 2;

int init_edge_detector(void) {
    // Initialize mouse position to center of screen
    mouse_x = SCREEN_WIDTH / 2;
    mouse_y = SCREEN_HEIGHT / 2;

    printf("Edge detector initialized (screen: %dx%d)\n", SCREEN_WIDTH, SCREEN_HEIGHT);
    return 0;
}

int is_at_edge(int *dx, int *dy) {
    *dx = 0;
    *dy = 0;

    if (mouse_x <= EDGE_THRESHOLD) {
        *dx = -1; // Left edge
        return 1;
    } else if (mouse_x >= SCREEN_WIDTH - EDGE_THRESHOLD) {
        *dx = 1; // Right edge
        return 1;
    }

    if (mouse_y <= EDGE_THRESHOLD) {
        *dy = -1; // Top edge
        return 1;
    } else if (mouse_y >= SCREEN_HEIGHT - EDGE_THRESHOLD) {
        *dy = 1; // Bottom edge
        return 1;
    }

    return 0;
}

void update_mouse_position(int dx, int dy) {
    mouse_x += dx;
    mouse_y += dy;

    // Keep mouse within screen bounds
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x >= SCREEN_WIDTH) mouse_x = SCREEN_WIDTH - 1;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y >= SCREEN_HEIGHT) mouse_y = SCREEN_HEIGHT - 1;
}

void cleanup_edge_detector(void) {
    // Nothing to cleanup for this simple implementation
}