#pragma once

#include <furi.h>

#include "walk.h"

/* Wander: the cursor drifts around without stopping, the way a desktop pet
 * would. It steers towards a random spot near the start, turning smoothly
 * and changing speed as it goes, picks another spot before it gets there,
 * and never follows a fixed shape. It stays within the walk length's reach
 * of the start; told to go home, it glides back to exactly where it began.
 *
 * Unlike a walk it is not planned ahead: each call to wander_step works out
 * the next 20 ms move from where the cursor is now.
 *
 * Given an area (the middle of the active window, from the desktop app), it
 * only picks spots inside it, and drifts into it when it starts outside. */

typedef struct {
    float x, y; /* where the cursor is meant to be, relative to the start */
    float heading; /* radians */
    float speed; /* counts per step */
    float cruise; /* the speed it is easing towards */
    float tx, ty; /* the spot it is steering for */
    uint16_t target_steps; /* steps since it picked that spot */
    int16_t sent_x, sent_y; /* where the moves sent so far put the cursor */
    float range;
    bool bounded; /* only picks spots inside the area below */
    float left, top, right, bottom; /* relative to the start */
    bool homing;
    uint16_t home_steps; /* steps since it set off home */
} Wander;

/* A wander told to go home gets there within this many steps (4.4 s). */
#define WANDER_HOME_STEPS 220

void wander_start(Wander* wander, WalkLength length);

/* A new reach for a wander under way, from where it is now. */
void wander_set_length(Wander* wander, WalkLength length);

/* The move for the next step, in whole counts. Rounding is carried, so the
 * moves always add up to the position. */
WalkPoint wander_step(Wander* wander);

/* Keep to this area from now on: edges relative to the start, the right
 * and bottom ones just outside. A spot outside it is given up at once. */
void wander_set_area(Wander* wander, int16_t left, int16_t top, int16_t right, int16_t bottom);

/* The pointer turned out to be (dx, dy) away from where the moves sent so
 * far put it (pointer acceleration turns counts into pixels at a rate that
 * depends on speed): carry on from where it really is. Nothing moves now;
 * a wander home by the count sets off again for the last few pixels. */
void wander_shift(Wander* wander, int16_t dx, int16_t dy);

/* Glide back to the start from here on. */
void wander_go_home(Wander* wander);

/* True once a homing wander is back at the start. */
bool wander_is_home(const Wander* wander);

/* Where the moves sent so far put the cursor, relative to the start. */
WalkPoint wander_position(const Wander* wander);

/* How far a wander strays from the start, for scaling it onto the screen. */
int16_t wander_reach(const Wander* wander);

/* A few seconds of wandering and the way home, as a path Preview can draw
 * and play. */
void wander_sample(WalkPath* path, WalkLength length);
