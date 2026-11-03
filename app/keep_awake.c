#include "keep_awake.h"
#include "usb/usb_duck.h"

#include <furi_hal_random.h>

static uint32_t random_between(uint32_t lo, uint32_t hi) {
    if(hi <= lo) return lo;
    return lo + furi_hal_random_get() % (hi - lo + 1);
}

static void schedule(KeepAwake* ka) {
    ka->next_tick = furi_get_tick() + furi_ms_to_ticks(random_between(ka->min_s, ka->max_s) * 1000);
}

void keep_awake_init(KeepAwake* ka, uint32_t min_s, uint32_t max_s) {
    ka->mode = KeepAwakeOff;
    ka->min_s = min_s;
    ka->max_s = max_s;
    ka->walk_length = WalkLengthMedium;
    ka->moves = 0;
    ka->paused = false;
    ka->step = 0;
    walk_plan(&ka->path, ka->walk_length);
    schedule(ka);
}

void keep_awake_set_mode(KeepAwake* ka, KeepAwakeMode mode) {
    ka->mode = mode;
    if(mode != KeepAwakeOff) schedule(ka);
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
    if(!ka->step) walk_plan(&ka->path, length);
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

void keep_awake_walk_now(KeepAwake* ka) {
    if(ka->step || !usb_duck_is_connected()) return;
    ka->step = 1;
    ka->step_tick = furi_get_tick();
}

bool keep_awake_is_walking(const KeepAwake* ka) {
    return ka->step != 0;
}

WalkPoint keep_awake_position(const KeepAwake* ka) {
    if(!ka->step) return (WalkPoint){0, 0};
    return ka->path.points[ka->step - 1];
}

/* Mouse reports carry at most 127 counts per axis. */
static bool move_by(int16_t dx, int16_t dy) {
    bool ok = true;
    while(ok && (dx || dy)) {
        int8_t sx = dx > 127 ? 127 : dx < -127 ? -127 : (int8_t)dx;
        int8_t sy = dy > 127 ? 127 : dy < -127 ? -127 : (int8_t)dy;
        ok = usb_duck_mouse_move(sx, sy);
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
    return usb_duck_mouse_move(dx, dy) && usb_duck_mouse_move(-dx, -dy);
}

static bool walk_step(KeepAwake* ka) {
    bool moved = false;
    while(ka->step && (int32_t)(furi_get_tick() - ka->step_tick) >= 0) {
        WalkPoint from = ka->path.points[ka->step - 1];
        WalkPoint to = ka->path.points[ka->step];
        moved |= move_by(to.x - from.x, to.y - from.y);
        ka->step_tick += furi_ms_to_ticks(WALK_STEP_MS);
        if(++ka->step >= ka->path.count) {
            ka->step = 0;
            ka->moves++;
            walk_plan(&ka->path, ka->walk_length);
            schedule(ka);
        }
    }
    return moved;
}

bool keep_awake_tick(KeepAwake* ka) {
    if(ka->step) return walk_step(ka);
    if(ka->mode == KeepAwakeOff || ka->paused) return false;
    if((int32_t)(furi_get_tick() - ka->next_tick) < 0) return false;

    if(ka->mode == KeepAwakeWalk) {
        keep_awake_walk_now(ka);
        if(ka->step) return walk_step(ka);
        schedule(ka);
        return false;
    }
    bool sent = usb_duck_is_connected() && nudge();
    if(sent) ka->moves++;
    schedule(ka);
    return sent;
}
