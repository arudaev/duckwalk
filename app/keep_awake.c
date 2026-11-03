#include "keep_awake.h"
#include "link.h"
#include "util.h"

#include <furi_hal_random.h>

/* After the Remote moves the pointer, a wander keeps off it this long. */
#define REMOTE_HOLD_MS 30000

static uint32_t random_between(uint32_t lo, uint32_t hi) {
    if(hi <= lo) return lo;
    return lo + furi_hal_random_get() % (hi - lo + 1);
}

static void schedule(KeepAwake* ka) {
    ka->next_tick = furi_get_tick();
    /* Wander has no wait: it sets off again as soon as it may. */
    if(ka->mode == KeepAwakeWander) return;
    ka->next_tick += furi_ms_to_ticks(random_between(ka->min_s, ka->max_s) * 1000);
}

void keep_awake_init(KeepAwake* ka, uint32_t min_s, uint32_t max_s) {
    ka->mode = KeepAwakeOff;
    ka->min_s = min_s;
    ka->max_s = max_s;
    ka->walk_length = WalkLengthMedium;
    ka->moves = 0;
    ka->paused = false;
    ka->user_active = false;
    ka->step = 0;
    ka->wandering = false;
    walk_plan(&ka->path, ka->walk_length);
    schedule(ka);
}

/* The path Preview shows and OK plays: a walk, or a sample of wandering. */
static void plan(KeepAwake* ka) {
    if(ka->mode == KeepAwakeWander) {
        wander_sample(&ka->path, ka->walk_length);
    } else {
        walk_plan(&ka->path, ka->walk_length);
    }
}

/* Take the pointer back from a wander where it is, without going home:
 * the user or another movement needs it now. */
static void drop_wander(KeepAwake* ka) {
    if(!ka->wandering) return;
    ka->wandering = false;
    ka->moves++;
}

void keep_awake_set_mode(KeepAwake* ka, KeepAwakeMode mode) {
    bool was_wander = ka->mode == KeepAwakeWander;
    ka->mode = mode;
    if(mode != KeepAwakeOff) schedule(ka);
    if(!ka->step && was_wander != (mode == KeepAwakeWander)) plan(ka);
}

void keep_awake_set_interval(KeepAwake* ka, uint32_t min_s, uint32_t max_s) {
    if(ka->min_s == min_s && ka->max_s == max_s) return;
    ka->min_s = min_s;
    ka->max_s = max_s;
    schedule(ka);
}

void keep_awake_set_walk_length(KeepAwake* ka, WalkLength length) {
    if(ka->walk_length == length) return;
    ka->walk_length = length;
    if(!ka->step) plan(ka);
}

void keep_awake_set_paused(KeepAwake* ka, bool paused) {
    if(ka->paused && !paused) schedule(ka);
    ka->paused = paused;
}

uint32_t keep_awake_seconds_left(const KeepAwake* ka) {
    if(ka->mode == KeepAwakeOff) return 0;
    int32_t left = (int32_t)(ka->next_tick - furi_get_tick());
    return left > 0 ? (uint32_t)left / furi_kernel_get_tick_frequency() : 0;
}

void keep_awake_set_user_active(KeepAwake* ka, bool active) {
    if(active == ka->user_active) return;
    ka->user_active = active;
    if(active && ka->step) {
        /* Let go of the pointer: the user has it now. */
        ka->step = 0;
        plan(ka);
    }
    if(active) drop_wander(ka);
    if(!active) schedule(ka);
}

void keep_awake_postpone(KeepAwake* ka) {
    if(ka->step) return;
    schedule(ka);
    if(ka->mode == KeepAwakeWander) {
        /* The Remote has the pointer: let go and keep off it a while. */
        drop_wander(ka);
        ka->next_tick += furi_ms_to_ticks(REMOTE_HOLD_MS);
    }
}

void keep_awake_replan(KeepAwake* ka) {
    if(!ka->step) plan(ka);
}

void keep_awake_play_circle(KeepAwake* ka) {
    if(ka->step || !link_is_connected()) return;
    walk_circle(&ka->path, 24, 40);
    ka->step = 1;
    ka->step_tick = furi_get_tick();
}

void keep_awake_walk_now(KeepAwake* ka) {
    if(ka->step || !link_is_connected()) return;
    ka->step = 1;
    ka->step_tick = furi_get_tick();
}

bool keep_awake_is_walking(const KeepAwake* ka) {
    return ka->step != 0 || ka->wandering;
}

bool keep_awake_is_playing(const KeepAwake* ka) {
    return ka->step != 0;
}

bool keep_awake_is_wandering(const KeepAwake* ka) {
    return ka->wandering;
}

WalkPoint keep_awake_position(const KeepAwake* ka) {
    if(ka->step) return ka->path.points[ka->step - 1];
    if(ka->wandering) return wander_position(&ka->wander);
    return (WalkPoint){0, 0};
}

int16_t keep_awake_reach(const KeepAwake* ka) {
    if(!ka->step && ka->wandering) return wander_reach(&ka->wander);
    return walk_reach(&ka->path);
}

bool keep_awake_let_go(KeepAwake* ka) {
    if(ka->wandering) wander_go_home(&ka->wander);
    return !keep_awake_is_walking(ka);
}

/* Mouse reports carry at most 127 counts per axis. */
static bool move_by(int16_t dx, int16_t dy) {
    bool ok = true;
    while(ok && (dx || dy)) {
        int8_t sx = dx > 127 ? 127 : dx < -127 ? -127 : (int8_t)dx;
        int8_t sy = dy > 127 ? 127 : dy < -127 ? -127 : (int8_t)dy;
        ok = link_mouse_move(sx, sy);
        dx -= sx;
        dy -= sy;
    }
    return ok;
}

/* One pixel out and back, in a random direction, so the pointer never drifts
 * and never follows the same pattern. */
static bool nudge(void) {
    int8_t dx = 0, dy = 0;
    switch(furi_hal_random_get() % 4) {
    case 0:
        dx = 1;
        break;
    case 1:
        dx = -1;
        break;
    case 2:
        dy = 1;
        break;
    default:
        dy = -1;
        break;
    }
    return link_mouse_move(dx, dy) && link_mouse_move(-dx, -dy);
}

static bool walk_step(KeepAwake* ka) {
    bool moved = false;
    while(ka->step && tick_reached(ka->step_tick)) {
        WalkPoint from = ka->path.points[ka->step - 1];
        WalkPoint to = ka->path.points[ka->step];
        moved |= move_by(to.x - from.x, to.y - from.y);
        ka->step_tick += furi_ms_to_ticks(WALK_STEP_MS);
        if(++ka->step >= ka->path.count) {
            ka->step = 0;
            ka->moves++;
            plan(ka);
            schedule(ka);
        }
    }
    return moved;
}

static bool wander_tick(KeepAwake* ka) {
    if(!link_is_connected()) {
        drop_wander(ka);
        return false;
    }
    /* The mode ended or work hours are over: glide home, then stop. */
    if(ka->mode != KeepAwakeWander || ka->paused) wander_go_home(&ka->wander);
    /* After a stall (a slow link change, say), carry on from now rather
     * than send every missed step at once. */
    if(tick_reached(ka->step_tick + furi_ms_to_ticks(WALK_STEP_MS * 5))) ka->step_tick = furi_get_tick();
    bool moved = false;
    while(ka->wandering && tick_reached(ka->step_tick)) {
        WalkPoint m = wander_step(&ka->wander);
        moved |= move_by(m.x, m.y);
        ka->step_tick += furi_ms_to_ticks(WALK_STEP_MS);
        if(wander_is_home(&ka->wander)) {
            drop_wander(ka);
            schedule(ka);
        }
    }
    return moved;
}

bool keep_awake_tick(KeepAwake* ka) {
    if(ka->step) return walk_step(ka);
    if(ka->wandering) return wander_tick(ka);
    if(ka->mode == KeepAwakeOff || ka->paused || ka->user_active) return false;
    if(tick_before(ka->next_tick)) return false;

    if(ka->mode == KeepAwakeWander) {
        if(!link_is_connected()) return false;
        wander_start(&ka->wander, ka->walk_length);
        ka->wandering = true;
        ka->step_tick = furi_get_tick();
        return wander_tick(ka);
    }
    if(ka->mode == KeepAwakeWalk) {
        keep_awake_walk_now(ka);
        if(ka->step) return walk_step(ka);
        schedule(ka);
        return false;
    }
    bool sent = link_is_connected() && nudge();
    if(sent) ka->moves++;
    schedule(ka);
    return sent;
}
