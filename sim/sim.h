#pragma once

/* The Duckwalk screen simulator: runs the app's real draw code on a
 * computer. See sim/README.md. */

#include <furi.h>
#include <gui/gui.h>

#include "link.h"

#define SCREEN_W 128
#define SCREEN_H 64

/* canvas.c */
Canvas* sim_canvas_get(void);
void sim_canvas_begin(const char* scene);
bool sim_canvas_pixel(int x, int y);
/* Strings drawn past the screen edge since sim_canvas_begin, one per line. */
const char* sim_canvas_overflow(void);

/* icons.c: load every app/images sprite; false if one is missing. */
bool sim_icons_load(void);

/* link_fake.c: what the fake connection reports. */
typedef struct {
    LinkKind kind;
    bool connected;
    bool on_air; /* Bluetooth advertising or connected */
} SimLink;
extern SimLink sim_link;
