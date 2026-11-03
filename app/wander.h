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
 * the next 20 ms move from where the cursor is now. */

typedef struct {
    float x, y; /* where the cursor is meant to be, relative to the start */
    float heading; /* radians */
    float speed; /* counts per step */
    float cruise; /* the speed it is easing towards */
    float tx, ty; /* the spot it is steering for */
    uint16_t target_steps; /* steps since it picked that spot */
    int16_t sent_x, sent_y; /* where the moves sent so far put the cursor */
    float range;
    bool homing;
    uint16_t home_steps; /* steps since it set off home */
} Wander;

/* A wander told to go home gets there within this many steps (4.4 s). */
#define WANDER_HOME_STEPS 220

void wander_start(Wander* wander, WalkLength length);

/* The move for the next step, in whole counts. Rounding is carried, so the
 * moves always add up to the position. */
WalkPoint wander_step(Wander* wander);

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
