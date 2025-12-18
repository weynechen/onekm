#include "edge_detector.h"
#include <stdio.h>
#include <stdlib.h>

// Screen dimensions
#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080
#define EDGE_THRESHOLD 5

static int mouse_x = SCREEN_WIDTH / 2;
static int mouse_y = SCREEN_HEIGHT / 2;

// State tracking to prevent immediate re-trigger
static int left_blocked = 0;
static int right_blocked = 0;
static int top_blocked = 0;
static int bottom_blocked = 0;

int init_edge_detector(void) {
    mouse_x = SCREEN_WIDTH / 2;
    mouse_y = SCREEN_HEIGHT / 2;
    left_blocked = 0;
    right_blocked = 0;
    top_blocked = 0;
    bottom_blocked = 0;
    printf("Edge detector initialized (screen: %dx%d)\n", SCREEN_WIDTH, SCREEN_HEIGHT);
    return 0;
}

int is_at_edge(int *dx, int *dy) {
    *dx = 0;
    *dy = 0;

    // Check left edge
    if (mouse_x <= EDGE_THRESHOLD) {
        if (!left_blocked) {
            *dx = -1;
            return 1;
        }
    } else if (mouse_x > EDGE_THRESHOLD + 20) {
        left_blocked = 0;  // Unblocked when away from edge
    }

    // Check right edge
    if (mouse_x >= SCREEN_WIDTH - EDGE_THRESHOLD) {
        if (!right_blocked) {
            *dx = 1;
            return 1;
        }
    } else if (mouse_x < SCREEN_WIDTH - EDGE_THRESHOLD - 20) {
        right_blocked = 0;
    }

    // Check top edge
    if (mouse_y <= EDGE_THRESHOLD) {
        if (!top_blocked) {
            *dy = -1;
            return 1;
        }
    } else if (mouse_y > EDGE_THRESHOLD + 20) {
        top_blocked = 0;
    }

    // Check bottom edge
    if (mouse_y >= SCREEN_HEIGHT - EDGE_THRESHOLD) {
        if (!bottom_blocked) {
            *dy = 1;
            return 1;
        }
    } else if (mouse_y < SCREEN_HEIGHT - EDGE_THRESHOLD - 20) {
        bottom_blocked = 0;
    }

    return 0;
}

void update_mouse_position(int dx, int dy) {
    mouse_x += dx;
    mouse_y += dy;

    // Keep within bounds
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x >= SCREEN_WIDTH) mouse_x = SCREEN_WIDTH - 1;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y >= SCREEN_HEIGHT) mouse_y = SCREEN_HEIGHT - 1;
}

void cleanup_edge_detector(void) {
    // Nothing to cleanup
}

// Call this when switching TO remote mode
void start_remote_mode(void) {
    // Reset position to center so user has space to move back
    mouse_x = SCREEN_WIDTH / 2;
    mouse_y = SCREEN_HEIGHT / 2;
    // Block all edges initially to prevent immediate trigger
    left_blocked = 1;
    right_blocked = 1;
    top_blocked = 1;
    bottom_blocked = 1;
}

// Call this when switching FROM remote mode
void end_remote_mode(void) {
    // Unblock edges for normal local mode
    left_blocked = 0;
    right_blocked = 0;
    top_blocked = 0;
    bottom_blocked = 0;
}
