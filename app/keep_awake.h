#pragma once

#include <furi.h>

#include "walk.h"
#include "wander.h"

/* Keep-awake scheduling. At a random moment inside [min_s, max_s] seconds:
 *   Subtle – move the pointer one pixel and back, so it never drifts.
 *   Walk   – play the planned walk (see walk.h), then plan the next one.
 *   Wander – no waiting: the cursor drifts around for as long as the mode
 *            is on (see wander.h) and glides back to its start when it ends.
 * The OS idle timer resets on any input, so any of them keeps the computer
 * awake. */

typedef enum {
    KeepAwakeOff,
    KeepAwakeSubtle,
    KeepAwakeWalk,
    KeepAwakeWander,
    KeepAwakeModeCount,
} KeepAwakeMode;

typedef struct {
    KeepAwakeMode mode;
    uint32_t min_s;
    uint32_t max_s;
    WalkLength walk_length;
    uint32_t next_tick;
    uint32_t moves;
    bool paused; /* outside the hours set in Settings */
    bool user_active; /* the PC helper sees the user working */
    WalkPath path; /* the next walk, or the one playing */
    uint16_t step; /* next point to play; 0 when not walking */
    uint32_t step_tick;
    Wander wander;
    bool wandering;
} KeepAwake;

void keep_awake_init(KeepAwake* ka, uint32_t min_s, uint32_t max_s);
void keep_awake_set_mode(KeepAwake* ka, KeepAwakeMode mode);
void keep_awake_set_interval(KeepAwake* ka, uint32_t min_s, uint32_t max_s);

/* Changing the length plans a new walk, so Preview always shows what will
 * actually play. */
void keep_awake_set_walk_length(KeepAwake* ka, WalkLength length);

/* While paused nothing is scheduled; resuming starts a fresh wait. */
void keep_awake_set_paused(KeepAwake* ka, bool paused);

/* Seconds until the next nudge or walk (0 when off). */
uint32_t keep_awake_seconds_left(const KeepAwake* ka);

/* While the user works, nothing is scheduled and a walk or wander in
 * progress stops on the spot; once they stop, a fresh wait starts. */
void keep_awake_set_user_active(KeepAwake* ka, bool active);

/* Start the wait over, e.g. after the user sent input from the remote. A
 * wander lets go of the pointer and waits 30 s. */
void keep_awake_postpone(KeepAwake* ka);

/* Throw the planned walk away and plan another (not while walking). */
void keep_awake_replan(KeepAwake* ka);

/* Play a small circle now (the pairing screen's pointer test). The planned
 * walk is replaced; a new one is planned when the circle ends. A wander
 * waits where it is and carries on afterwards. */
void keep_awake_play_circle(KeepAwake* ka);

/* Play the planned walk now, whatever the mode (Preview uses this). */
void keep_awake_walk_now(KeepAwake* ka);

/* True while Harold has the pointer: a walk, the circle or a wander. */
bool keep_awake_is_walking(const KeepAwake* ka);

/* True while a walk or the circle plays (not a wander). */
bool keep_awake_is_playing(const KeepAwake* ka);

bool keep_awake_is_wandering(const KeepAwake* ka);

/* Where the cursor is on the current walk, relative to its start. */
WalkPoint keep_awake_position(const KeepAwake* ka);

/* How far the current walk or wander strays, for scaling it on screen. */
int16_t keep_awake_reach(const KeepAwake* ka);

/* Leaving the app: a wander glides home first. Returns true once nothing
 * is moving; call keep_awake_tick until then. */
bool keep_awake_let_go(KeepAwake* ka);

/* Call at least every WALK_STEP_MS. Returns true when it moved the pointer. */
bool keep_awake_tick(KeepAwake* ka);
