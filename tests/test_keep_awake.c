/* Keep-awake in Wander mode: it starts at once, hands the pointer to the
 * circle test and the Remote, and always ends where it started. */

#include "check.h"
#include "keep_awake.h"

#include <furi_hal_random.h>
#include <math.h>
#include <stdlib.h>

extern int fake_link_x;
extern int fake_link_y;
extern bool fake_link_connected;

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

/* A computer with pointer acceleration: a fast step goes further per count
 * than a slow one. It reports the pointer and a window to the Flipper every
 * HELPER_AREA_EVERY_MS, as the desktop app does. */
static float pixel_x, pixel_y;

static float accelerate(int d) {
    return abs(d) >= 3 ? d * 1.3f : abs(d) == 2 ? d * 1.1f : d * 0.8f;
}

static void run_with_pc(KeepAwake* ka, uint32_t ms) {
    for(uint32_t t = 0; t < ms; t += WALK_STEP_MS) {
        now += WALK_STEP_MS;
        shim_tick_set(now);
        int x = fake_link_x, y = fake_link_y;
        keep_awake_tick(ka);
        pixel_x += accelerate(fake_link_x - x);
        pixel_y += accelerate(fake_link_y - y);
        if(t % 100 == 0) {
            PointerArea a = {
                .x = (int16_t)lroundf(pixel_x),
                .y = (int16_t)lroundf(pixel_y),
                .safe = {600, 400, 1400, 900},
                .screen = {0, 0, 1920, 1080},
            };
            keep_awake_set_area(ka, &a, now);
        }
    }
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

        /* Another mode: it glides home within the time the app allows. */
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

    /* A new walk length takes hold mid-wander, and it still ends at the
     * start. */
    wander_mode(&ka);
    keep_awake_set_walk_length(&ka, WalkLengthShort);
    run_for(&ka, 1000);
    int16_t short_reach = wander_reach(&ka.wander);
    keep_awake_set_walk_length(&ka, WalkLengthLong);
    CHECK(wander_reach(&ka.wander) > short_reach, "reach didn't grow with the walk length");
    int farthest = 0;
    for(int i = 0; i < 60000 / WALK_STEP_MS; i++) {
        run_for(&ka, WALK_STEP_MS);
        int d = abs(fake_link_x) > abs(fake_link_y) ? abs(fake_link_x) : abs(fake_link_y);
        if(d > farthest) farthest = d;
    }
    CHECK(farthest > short_reach, "stayed within the short reach (%d) after Long", farthest);
    keep_awake_set_mode(&ka, KeepAwakeSubtle);
    run_for(&ka, WANDER_HOME_STEPS * WALK_STEP_MS);
    CHECK(fake_link_x == 0 && fake_link_y == 0, "ended at (%d,%d) after a length change", fake_link_x, fake_link_y);

    /* The PC is away for a month: Wander picks up again when it is back,
     * even after the tick difference wraps at 24.8 days. */
    wander_mode(&ka);
    run_for(&ka, 1000);
    fake_link_connected = false;
    for(int hour = 0; hour < 30 * 24; hour++) {
        now += 3600000;
        shim_tick_set(now);
        keep_awake_tick(&ka);
    }
    fake_link_connected = true;
    run_for(&ka, 200);
    CHECK(keep_awake_is_wandering(&ka), "didn't wander again after a month without the PC");

    /* With the desktop app: the wander keeps to the window and comes home
     * to the very pixel it set off from, despite pointer acceleration. */
    int landed = 0;
    for(uint32_t seed = 1; seed <= 50; seed++) {
        shim_random_seed(seed * 2246822519u);
        pixel_x = 1000;
        pixel_y = 650;
        fake_link_x = fake_link_y = 0;
        keep_awake_init(&ka, 30, 60);
        keep_awake_set_walk_length(&ka, WalkLengthLong);
        run_with_pc(&ka, 200); /* a report in before it sets off */
        keep_awake_set_mode(&ka, KeepAwakeWander);
        bool outside = false;
        for(int s = 0; s < 30 * 10; s++) {
            run_with_pc(&ka, 100);
            outside |= pixel_x < 600 - 60 || pixel_x >= 1400 + 60 || pixel_y < 400 - 60 || pixel_y >= 900 + 60;
        }
        CHECK(!outside, "seed %lu: left the window", (unsigned long)seed);
        keep_awake_set_mode(&ka, KeepAwakeSubtle);
        run_with_pc(&ka, WANDER_HOME_STEPS * WALK_STEP_MS + 4000);
        CHECK(!keep_awake_is_walking(&ka), "seed %lu: still walking", (unsigned long)seed);
        int ex = (int)lroundf(pixel_x) - 1000, ey = (int)lroundf(pixel_y) - 650;
        CHECK(abs(ex) <= 1 && abs(ey) <= 1, "seed %lu: home %d,%d px off", (unsigned long)seed, ex, ey);
        landed += ex == 0 && ey == 0;
    }
    printf("landed on the very pixel %d times in 50\n", landed);

    /* Next to the right edge, a walk goes left. */
    shim_random_seed(11);
    keep_awake_init(&ka, 30, 60);
    keep_awake_set_walk_length(&ka, WalkLengthLong);
    keep_awake_set_mode(&ka, KeepAwakeWalk);
    PointerArea edge = {.x = 1910, .y = 500, .safe = {600, 400, 1400, 900}, .screen = {0, 0, 1920, 1080}};
    keep_awake_set_area(&ka, &edge, now);
    keep_awake_walk_now(&ka);
    CHECK(ka.path.max_x < 10 && ka.path.min_x >= -1910, "walk runs %d past the right edge", ka.path.max_x - 9);

    return check_result("keep_awake");
}
