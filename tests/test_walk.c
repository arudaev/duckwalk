/* Walk planner: every planned walk is safe to play and never the same twice. */

#include "check.h"
#include "walk.h"

#include <furi_hal_random.h>
#include <stdlib.h>

#define PLANS 1000

/* Nominal steps per length; must match walk_lengths in walk.c. */
static const uint16_t nominal_steps[WalkLengthCount] = {90, 150, 220};

static uint32_t path_hash(const WalkPath* path) {
    uint32_t h = 2166136261u;
    for(uint16_t i = 0; i < path->count; i++) {
        h = (h ^ (uint16_t)path->points[i].x) * 16777619u;
        h = (h ^ (uint16_t)path->points[i].y) * 16777619u;
    }
    return h;
}

static int compare_u32(const void* a, const void* b) {
    uint32_t x = *(const uint32_t*)a, y = *(const uint32_t*)b;
    return x < y ? -1 : x > y;
}

static void check_one(const WalkPath* path, WalkLength length, uint32_t seed) {
    CHECK(path->count > 10 && path->count <= WALK_MAX_STEPS, "seed %lu: %u steps", (unsigned long)seed, path->count);
    int over = path->count - nominal_steps[length];
    CHECK(over > -10 && over < 30, "seed %lu: %u steps for nominal %u", (unsigned long)seed, path->count, nominal_steps[length]);

    WalkPoint first = path->points[0], last = path->points[path->count - 1];
    CHECK(first.x == 0 && first.y == 0, "seed %lu: starts at (%d,%d)", (unsigned long)seed, first.x, first.y);
    CHECK(abs(last.x) <= 2 && abs(last.y) <= 2, "seed %lu: ends at (%d,%d)", (unsigned long)seed, last.x, last.y);

    int16_t min_x = 0, max_x = 0, min_y = 0, max_y = 0;
    int longest_hold = 0, hold = 0;
    for(uint16_t i = 1; i < path->count; i++) {
        WalkPoint a = path->points[i - 1], b = path->points[i];
        /* One mouse report carries at most 127 counts per axis; the player
         * splits bigger moves, but a walk should never need to. */
        CHECK(abs(b.x - a.x) <= 127 && abs(b.y - a.y) <= 127, "seed %lu: jump at step %u", (unsigned long)seed, i);
        if(b.x < min_x) min_x = b.x;
        if(b.x > max_x) max_x = b.x;
        if(b.y < min_y) min_y = b.y;
        if(b.y > max_y) max_y = b.y;
        hold = (a.x == b.x && a.y == b.y) ? hold + 1 : 0;
        if(hold > longest_hold) longest_hold = hold;
    }
    CHECK(min_x == path->min_x && max_x == path->max_x && min_y == path->min_y && max_y == path->max_y,
          "seed %lu: bounding box", (unsigned long)seed);
    CHECK(walk_reach(path) > 0, "seed %lu: reach 0", (unsigned long)seed);
    /* The planned pause: at least 8 steps standing still. */
    CHECK(longest_hold >= 7, "seed %lu: longest pause %d steps", (unsigned long)seed, longest_hold);
}

int main(void) {
    static WalkPath path;
    static uint32_t hashes[PLANS];

    for(WalkLength length = 0; length < WalkLengthCount; length++) {
        int16_t widest = 0;
        for(uint32_t seed = 1; seed <= PLANS; seed++) {
            shim_random_seed(seed * 2654435761u);
            walk_plan(&path, length);
            check_one(&path, length, seed);
            hashes[seed - 1] = path_hash(&path);
            if(walk_reach(&path) > widest) widest = walk_reach(&path);
        }
        qsort(hashes, PLANS, sizeof(hashes[0]), compare_u32);
        int repeats = 0;
        for(int i = 1; i < PLANS; i++) repeats += hashes[i] == hashes[i - 1];
        CHECK(repeats == 0, "length %d: %d repeated paths in %d", length, repeats, PLANS);
        printf("length %d: widest reach %d counts\n", length, widest);
    }

    /* Longer settings walk farther. */
    int16_t reach[WalkLengthCount] = {0};
    for(WalkLength length = 0; length < WalkLengthCount; length++) {
        int32_t sum = 0;
        for(uint32_t seed = 1; seed <= 200; seed++) {
            shim_random_seed(seed);
            walk_plan(&path, length);
            sum += walk_reach(&path);
        }
        reach[length] = sum / 200;
    }
    CHECK(reach[WalkLengthShort] < reach[WalkLengthMedium] && reach[WalkLengthMedium] < reach[WalkLengthLong],
          "average reach %d, %d, %d", reach[0], reach[1], reach[2]);

    return check_result("walk");
}
