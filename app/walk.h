#pragma once

#include <furi.h>

/* A walk: the cursor path Harold takes when you are away. A smooth curve
 * through a few random waypoints that slows down at each one, pauses once
 * and ends within two pixels of where it started. Planned ahead of time so
 * Preview can show the exact path before it is played. */

#define WALK_STEP_MS   20
#define WALK_MAX_STEPS 384

typedef enum {
    WalkLengthShort,
    WalkLengthMedium,
    WalkLengthLong,
    WalkLengthCount,
} WalkLength;

typedef struct {
    int16_t x;
    int16_t y;
} WalkPoint;

typedef struct {
    WalkPoint points[WALK_MAX_STEPS]; /* cursor offset from the start, one per step */
    uint16_t count;
    uint16_t pause_at; /* the first step of the pause; count when there is none */
    int16_t min_x, max_x, min_y, max_y;
} WalkPath;

void walk_plan(WalkPath* path, WalkLength length);

/* A circle of the given radius that starts and ends at the start, drawn in
 * `steps` steps. The pairing screen's pointer test. */
void walk_circle(WalkPath* path, int16_t radius, uint16_t steps);

/* Keep the path on the screen: the room around the start, in pixels, the
 * right and bottom edges just outside. It is mirrored left to right or top
 * to bottom where that keeps more of it on, then squeezed on an axis that
 * still runs off. Returns true if it changed the path. */
bool walk_fit(WalkPath* path, int16_t left, int16_t top, int16_t right, int16_t bottom);

/* Widest distance from the start, for scaling the path onto the screen. */
int16_t walk_reach(const WalkPath* path);
