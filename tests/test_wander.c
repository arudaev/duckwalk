/* Wander: the cursor keeps moving, stays in range and always gets home. */

#include "check.h"
#include "wander.h"

#include <furi_hal_random.h>
#include <math.h>
#include <stdlib.h>

#define SEEDS 300
#define STEPS 3000 /* a minute of wandering per seed */

static void check_one(WalkLength length, uint32_t seed) {
    Wander w;
    wander_start(&w, length);
    int16_t x = 0, y = 0;
    int still = 0, longest_still = 0;
    float farthest = 0;
    for(int i = 0; i < STEPS; i++) {
        WalkPoint m = wander_step(&w);
        CHECK(abs(m.x) <= 127 && abs(m.y) <= 127, "seed %lu: jump at step %d", (unsigned long)seed, i);
        x += m.x;
        y += m.y;
        WalkPoint p = wander_position(&w);
        CHECK(p.x == x && p.y == y, "seed %lu: moves don't add up at step %d", (unsigned long)seed, i);
        float d = hypotf(x, y);
        if(d > farthest) farthest = d;
        /* Never stands still: at most a step or two while slow. */
        still = (m.x == 0 && m.y == 0) ? still + 1 : 0;
        if(i > 50 && still > longest_still) longest_still = still;
    }
    /* Turning takes room: up to a turning circle (a quarter of the range)
     * either side of it. */
    CHECK(farthest <= wander_reach(&w) * 1.5f + 5, "seed %lu: strayed %.0f for reach %d", (unsigned long)seed, farthest, wander_reach(&w));
    CHECK(farthest >= wander_reach(&w) * 0.6f, "seed %lu: only strayed %.0f", (unsigned long)seed, farthest);
    CHECK(longest_still <= 2, "seed %lu: stood still %d steps", (unsigned long)seed, longest_still);

    wander_go_home(&w);
    int steps = 0;
    while(!wander_is_home(&w) && steps < WANDER_HOME_STEPS) {
        WalkPoint m = wander_step(&w);
        x += m.x;
        y += m.y;
        steps++;
    }
    CHECK(wander_is_home(&w), "seed %lu: not home after %d steps", (unsigned long)seed, steps);
    CHECK(x == 0 && y == 0, "seed %lu: home at (%d,%d)", (unsigned long)seed, x, y);
}

/* Told to go home at any moment, from anywhere: it lands on the start
 * within the time the app waits on exit. */
static void check_homing(WalkLength length, uint32_t seed) {
    Wander w;
    wander_start(&w, length);
    int16_t x = 0, y = 0;
    int before = 1 + furi_hal_random_get() % 1500;
    for(int i = 0; i < before; i++) {
        WalkPoint m = wander_step(&w);
        x += m.x;
        y += m.y;
    }
    wander_go_home(&w);
    int steps = 0;
    while(!wander_is_home(&w) && steps < WANDER_HOME_STEPS) {
        WalkPoint m = wander_step(&w);
        /* No faster than the wander went out: pointer acceleration would
         * make a faster way home overshoot. */
        CHECK(abs(m.x) <= 4 && abs(m.y) <= 4, "seed %lu: rushed home (%d,%d)", (unsigned long)seed, m.x, m.y);
        x += m.x;
        y += m.y;
        steps++;
    }
    CHECK(wander_is_home(&w) && x == 0 && y == 0, "seed %lu, length %d: at (%d,%d) after %d steps home",
          (unsigned long)seed, length, x, y, steps);
}

static bool target_inside(const Wander* w, int16_t left, int16_t top, int16_t right, int16_t bottom) {
    return w->tx >= left && w->tx < right && w->ty >= top && w->ty < bottom;
}

/* Given an area, every spot it heads for is inside it, and the pointer
 * stays inside but for a turning circle; started outside, it drifts in. */
static void check_area(WalkLength length, uint32_t seed) {
    /* An area around the start, or one off to the side of it. */
    int16_t left, top, right, bottom;
    switch(seed % 4) {
    case 0: /* a big window around the pointer */
        left = -500, top = -300, right = 500, bottom = 300;
        break;
    case 1: /* a small window */
        left = -60, top = -40, right = 60, bottom = 40;
        break;
    case 2: /* the pointer next to the taskbar, the window above */
        left = -400, top = -500, right = 400, bottom = -80;
        break;
    default: /* the pointer far right of the window */
        left = -900, top = -200, right = -300, bottom = 200;
        break;
    }
    Wander w;
    wander_start(&w, length);
    wander_set_area(&w, left, top, right, bottom);
    int16_t x = 0, y = 0;
    bool inside = seed % 4 < 2, came_in = inside;
    int out_after_in = 0;
    /* How far a turn can carry it past an edge: its turning circle, at most
     * a quarter of the room it has. */
    float room = fminf(wander_reach(&w), fminf(right - left, bottom - top) / 2.0f);
    float swing = room / 2.0f + 5;
    for(int i = 0; i < STEPS; i++) {
        CHECK(target_inside(&w, left, top, right, bottom), "seed %lu: heads for (%.0f,%.0f) outside the area",
              (unsigned long)seed, w.tx, w.ty);
        WalkPoint m = wander_step(&w);
        x += m.x;
        y += m.y;
        bool in = x >= left && x < right && y >= top && y < bottom;
        if(in) came_in = true;
        bool near = x >= left - swing && x < right + swing && y >= top - swing && y < bottom + swing;
        if(came_in && !near) out_after_in++;
    }
    CHECK(came_in, "seed %lu: never got into the area", (unsigned long)seed);
    CHECK(out_after_in == 0, "seed %lu: strayed out of the area %d steps", (unsigned long)seed, out_after_in);

    wander_go_home(&w);
    int steps = 0;
    while(!wander_is_home(&w) && steps < WANDER_HOME_STEPS * 3) {
        WalkPoint m = wander_step(&w);
        x += m.x;
        y += m.y;
        steps++;
    }
    CHECK(wander_is_home(&w) && x == 0 && y == 0, "seed %lu: home at (%d,%d)", (unsigned long)seed, x, y);
}

/* Home by the count but a few pixels off: told so, it steps onto the start
 * slowly and lands on it. */
static void check_correct(uint32_t seed) {
    Wander w;
    wander_start(&w, WalkLengthMedium);
    wander_go_home(&w);
    int guard = 0;
    while(!wander_is_home(&w) && guard++ < 1000) wander_step(&w);
    int16_t dx = (int16_t)(seed % 81) - 40, dy = (int16_t)(seed % 37) - 18;
    wander_shift(&w, dx, dy);
    int16_t x = dx, y = dy;
    int steps = 0;
    while(!wander_is_home(&w) && steps < WANDER_HOME_STEPS) {
        WalkPoint m = wander_step(&w);
        CHECK(abs(m.x) <= 3 && abs(m.y) <= 3, "seed %lu: correction jumped (%d,%d)", (unsigned long)seed, m.x, m.y);
        x += m.x;
        y += m.y;
        steps++;
    }
    CHECK(wander_is_home(&w) && x == 0 && y == 0, "seed %lu: corrected from (%d,%d) to (%d,%d) in %d steps",
          (unsigned long)seed, dx, dy, x, y, steps);
}

int main(void) {
    for(WalkLength length = 0; length < WalkLengthCount; length++) {
        for(uint32_t seed = 1; seed <= 20000; seed++) {
            shim_random_seed(seed * 2654435761u + length);
            check_homing(length, seed);
        }
    }

    for(WalkLength length = 0; length < WalkLengthCount; length++) {
        for(uint32_t seed = 1; seed <= SEEDS; seed++) {
            shim_random_seed(seed * 2654435761u);
            check_one(length, seed);
        }
    }

    for(WalkLength length = 0; length < WalkLengthCount; length++) {
        for(uint32_t seed = 1; seed <= SEEDS; seed++) {
            shim_random_seed(seed * 2654435761u + 17);
            check_area(length, seed);
        }
    }
    for(uint32_t seed = 1; seed <= SEEDS; seed++) {
        shim_random_seed(seed);
        check_correct(seed);
    }

    /* Preview's sample fits a path, starts and ends at the start. */
    static WalkPath path;
    for(uint32_t seed = 1; seed <= SEEDS; seed++) {
        shim_random_seed(seed * 2246822519u);
        wander_sample(&path, WalkLengthLong);
        WalkPoint last = path.points[path.count - 1], before = path.points[path.count - 2];
        CHECK(path.count > 60 && path.count <= WALK_MAX_STEPS, "seed %lu: %u steps", (unsigned long)seed, path.count);
        CHECK(last.x == 0 && last.y == 0, "seed %lu: sample ends at (%d,%d)", (unsigned long)seed, last.x, last.y);
        CHECK(abs(before.x) <= 5 && abs(before.y) <= 5, "seed %lu: sample snaps home from (%d,%d)", (unsigned long)seed, before.x, before.y);
    }
    return check_result("wander");
}
