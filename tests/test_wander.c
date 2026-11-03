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
    while(!wander_is_home(&w) && steps < 400) {
        WalkPoint m = wander_step(&w);
        x += m.x;
        y += m.y;
        steps++;
    }
    CHECK(wander_is_home(&w), "seed %lu: not home after %d steps", (unsigned long)seed, steps);
    CHECK(x == 0 && y == 0, "seed %lu: home at (%d,%d)", (unsigned long)seed, x, y);
}

int main(void) {
    for(WalkLength length = 0; length < WalkLengthCount; length++) {
        for(uint32_t seed = 1; seed <= SEEDS; seed++) {
            shim_random_seed(seed * 2654435761u);
            check_one(length, seed);
        }
    }

    /* Preview's sample fits a path, starts and ends at the start. */
    static WalkPath path;
    for(uint32_t seed = 1; seed <= SEEDS; seed++) {
        shim_random_seed(seed * 2246822519u);
        wander_sample(&path, WalkLengthLong);
        WalkPoint last = path.points[path.count - 1], before = path.points[path.count - 2];
        CHECK(path.count > 100 && path.count <= WALK_MAX_STEPS, "seed %lu: %u steps", (unsigned long)seed, path.count);
        CHECK(last.x == 0 && last.y == 0, "seed %lu: sample ends at (%d,%d)", (unsigned long)seed, last.x, last.y);
        CHECK(abs(before.x) <= 5 && abs(before.y) <= 5, "seed %lu: sample snaps home from (%d,%d)", (unsigned long)seed, before.x, before.y);
    }
    return check_result("wander");
}
