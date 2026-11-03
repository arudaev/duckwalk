/* Keep-awake in Wander mode: it starts at once, hands the pointer to the
 * circle test and the Remote, and always ends where it started. */

#include "check.h"
#include "keep_awake.h"

#include <furi_hal_random.h>

extern int fake_link_x;
extern int fake_link_y;

static uint32_t now = 100000;

/* Let time pass the way the main loop would, one walk step at a time. */
static void run_for(KeepAwake* ka, uint32_t ms) {
    for(uint32_t t = 0; t < ms; t += WALK_STEP_MS) {
        now += WALK_STEP_MS;
        shim_tick_set(now);
        keep_awake_tick(ka);
    }
}

static void wander_mode(KeepAwake* ka) {
    fake_link_x = fake_link_y = 0;
    keep_awake_init(ka, 30, 60);
    keep_awake_set_walk_length(ka, WalkLengthLong);
    keep_awake_set_mode(ka, KeepAwakeWander);
}

int main(void) {
    static KeepAwake ka;
    shim_tick_set(now);

    for(uint32_t seed = 1; seed <= 200; seed++) {
        shim_random_seed(seed * 2654435761u);

        /* It sets off straight away and keeps going. */
        wander_mode(&ka);
        run_for(&ka, WALK_STEP_MS);
        CHECK(keep_awake_is_wandering(&ka), "seed %lu: didn't start", (unsigned long)seed);
        run_for(&ka, 2000 + seed * 37);
        CHECK(keep_awake_is_wandering(&ka), "seed %lu: stopped by itself", (unsigned long)seed);

        /* The pointer test: the circle plays, the wander waits and carries
         * on, and still finds its way home. */
        keep_awake_play_circle(&ka);
        CHECK(keep_awake_is_playing(&ka), "seed %lu: circle didn't play", (unsigned long)seed);
        run_for(&ka, 1500);
        CHECK(!keep_awake_is_playing(&ka), "seed %lu: circle didn't end", (unsigned long)seed);
        CHECK(keep_awake_is_wandering(&ka), "seed %lu: wander lost after the circle", (unsigned long)seed);
        run_for(&ka, 500);

        /* Another mode: it glides home within the 3 s the app allows. */
        keep_awake_set_mode(&ka, KeepAwakeSubtle);
        run_for(&ka, WANDER_HOME_STEPS * WALK_STEP_MS);
        CHECK(!keep_awake_is_walking(&ka), "seed %lu: still moving after mode change", (unsigned long)seed);
        CHECK(fake_link_x == 0 && fake_link_y == 0, "seed %lu: ended at (%d,%d)", (unsigned long)seed, fake_link_x, fake_link_y);

        /* Leaving the app does the same. */
        wander_mode(&ka);
        run_for(&ka, 1000 + seed * 13);
        int steps = 0;
        while(!keep_awake_let_go(&ka) && steps++ < WANDER_HOME_STEPS) run_for(&ka, WALK_STEP_MS);
        CHECK(fake_link_x == 0 && fake_link_y == 0, "seed %lu: left at (%d,%d)", (unsigned long)seed, fake_link_x, fake_link_y);
    }

    /* The Remote takes the pointer: the wander lets go and keeps off it. */
    wander_mode(&ka);
    run_for(&ka, 1000);
    keep_awake_postpone(&ka);
    CHECK(!keep_awake_is_wandering(&ka), "wander kept going under the Remote");
    int x = fake_link_x, y = fake_link_y;
    run_for(&ka, 29000);
    CHECK(fake_link_x == x && fake_link_y == y, "moved within 30 s of the Remote");
    run_for(&ka, 2000);
    CHECK(keep_awake_is_wandering(&ka), "didn't wander again after the hold");

    /* Work hours over: home, then nothing until they start again. */
    wander_mode(&ka);
    run_for(&ka, 1500);
    keep_awake_set_paused(&ka, true);
    run_for(&ka, WANDER_HOME_STEPS * WALK_STEP_MS);
    CHECK(!keep_awake_is_walking(&ka) && fake_link_x == 0 && fake_link_y == 0, "paused away from the start");
    run_for(&ka, 5000);
    CHECK(!keep_awake_is_walking(&ka), "wandered while paused");

    return check_result("keep_awake");
}
