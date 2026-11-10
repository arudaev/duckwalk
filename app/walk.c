#include "walk.h"

#include <furi_hal_random.h>
#include <math.h>

#define WALK_MAX_WAYPOINTS 6

typedef struct {
    float x;
    float y;
} Vec;

/* How far a walk reaches from the start, in mouse counts (pixels at the
 * default Windows pointer speed), and how many 20 ms steps it lasts. */
static const struct {
    float reach;
    uint16_t steps;
} walk_lengths[WalkLengthCount] = {
    [WalkLengthShort] = {60.0f, 90},
    [WalkLengthMedium] = {140.0f, 150},
    [WalkLengthLong] = {280.0f, 220},
};

static float random_float(float lo, float hi) {
    return lo + (hi - lo) * (float)(furi_hal_random_get() % 10001) / 10000.0f;
}

/* Catmull-Rom: a curve through p1 and p2 that bends towards p0 and p3, so
 * the path turns smoothly at every waypoint. */
static Vec spline(Vec p0, Vec p1, Vec p2, Vec p3, float t) {
    float t2 = t * t, t3 = t2 * t;
    Vec v;
    v.x = 0.5f * (2 * p1.x + (p2.x - p0.x) * t + (2 * p0.x - 5 * p1.x + 4 * p2.x - p3.x) * t2 +
                  (3 * p1.x - p0.x - 3 * p2.x + p3.x) * t3);
    v.y = 0.5f * (2 * p1.y + (p2.y - p0.y) * t + (2 * p0.y - 5 * p1.y + 4 * p2.y - p3.y) * t2 +
                  (3 * p1.y - p0.y - 3 * p2.y + p3.y) * t3);
    return v;
}

/* Slow in, slow out: the cursor eases to a stop at every waypoint, like a
 * hand that moves, settles and moves again. */
static float ease(float t) {
    return t * t * (3.0f - 2.0f * t);
}

static void add_point(WalkPath* path, Vec v) {
    if(path->count >= WALK_MAX_STEPS) return;
    WalkPoint p = {(int16_t)lroundf(v.x), (int16_t)lroundf(v.y)};
    path->points[path->count++] = p;
    if(p.x < path->min_x) path->min_x = p.x;
    if(p.x > path->max_x) path->max_x = p.x;
    if(p.y < path->min_y) path->min_y = p.y;
    if(p.y > path->max_y) path->max_y = p.y;
}

void walk_plan(WalkPath* path, WalkLength length) {
    furi_check(length < WalkLengthCount);
    /* The pause and the minimum steps per segment must fit in the budget. */
    furi_check(walk_lengths[length].steps > 20 + 4 * WALK_MAX_WAYPOINTS);
    const float reach = walk_lengths[length].reach;

    /* Waypoints sit around the start at random distances, each one a random
     * turn left or right from the last, so the walk loops back home. */
    Vec way[WALK_MAX_WAYPOINTS];
    size_t n = 0;
    way[n++] = (Vec){0, 0};
    size_t stops = 2 + furi_hal_random_get() % 2;
    float heading = random_float(0, 2 * (float)M_PI);
    for(size_t i = 0; i < stops; i++) {
        float dist = random_float(0.45f, 1.0f) * reach;
        way[n++] = (Vec){cosf(heading) * dist, sinf(heading) * dist};
        heading += random_float(0.6f, 1.6f) * (furi_hal_random_get() % 2 ? 1.0f : -1.0f);
    }
    way[n++] = (Vec){random_float(-2, 2), random_float(-2, 2)};

    size_t pause_at = 1 + furi_hal_random_get() % (n - 2);
    uint16_t pause_steps = 8 + furi_hal_random_get() % 13;

    float seg_len[WALK_MAX_WAYPOINTS - 1];
    float total = 0;
    for(size_t i = 0; i + 1 < n; i++) {
        seg_len[i] = hypotf(way[i + 1].x - way[i].x, way[i + 1].y - way[i].y);
        total += seg_len[i];
    }
    uint16_t move_steps = walk_lengths[length].steps - pause_steps;

    path->count = 0;
    path->min_x = path->max_x = path->min_y = path->max_y = 0;
    for(size_t s = 0; s + 1 < n; s++) {
        if(s == pause_at) {
            for(uint16_t k = 0; k < pause_steps; k++) add_point(path, way[s]);
        }
        Vec p0 = way[s > 0 ? s - 1 : 0];
        Vec p3 = way[s + 2 < n ? s + 2 : n - 1];
        uint16_t steps = (uint16_t)lroundf(move_steps * seg_len[s] / total);
        if(steps < 4) steps = 4;
        for(uint16_t k = 0; k < steps; k++) {
            add_point(path, spline(p0, way[s], way[s + 1], p3, ease((float)k / steps)));
        }
    }
    add_point(path, way[n - 1]);
}

void walk_circle(WalkPath* path, int16_t radius, uint16_t steps) {
    furi_check(steps > 0 && steps < WALK_MAX_STEPS);
    path->count = 0;
    path->min_x = path->max_x = path->min_y = path->max_y = 0;
    for(uint16_t k = 0; k <= steps; k++) {
        float a = 2.0f * (float)M_PI * k / steps;
        add_point(path, (Vec){radius * sinf(a), radius * (1.0f - cosf(a))});
    }
}

/* How far a path from `lo` to `hi` sticks out of [left, right). */
static int32_t overflow(int16_t lo, int16_t hi, int16_t left, int16_t right) {
    int32_t out = 0;
    if(lo < left) out += left - lo;
    if(hi >= right) out += hi - right + 1;
    return out;
}

/* How much of [lo, hi] fits the room, as a factor (1 when it all does). */
static float fits(int16_t lo, int16_t hi, int16_t left, int16_t right) {
    float f = 1.0f;
    if(hi >= right && hi > 0) f = fminf(f, (right - 1) / (float)hi);
    if(lo < left && lo < 0) f = fminf(f, left / (float)lo);
    return f < 0 ? 0 : f;
}

bool walk_fit(WalkPath* path, int16_t left, int16_t top, int16_t right, int16_t bottom) {
    bool flip_x = overflow(-path->max_x, -path->min_x, left, right) < overflow(path->min_x, path->max_x, left, right);
    bool flip_y = overflow(-path->max_y, -path->min_y, top, bottom) < overflow(path->min_y, path->max_y, top, bottom);
    float sx = flip_x ? -1.0f : 1.0f, sy = flip_y ? -1.0f : 1.0f;
    int16_t lo_x = flip_x ? -path->max_x : path->min_x, hi_x = flip_x ? -path->min_x : path->max_x;
    int16_t lo_y = flip_y ? -path->max_y : path->min_y, hi_y = flip_y ? -path->min_y : path->max_y;
    sx *= fits(lo_x, hi_x, left, right);
    sy *= fits(lo_y, hi_y, top, bottom);
    if(sx == 1.0f && sy == 1.0f) return false;
    path->min_x = path->max_x = path->min_y = path->max_y = 0;
    for(uint16_t i = 0; i < path->count; i++) {
        WalkPoint* p = &path->points[i];
        /* Towards zero, so a squeezed point never lands past the edge. */
        p->x = (int16_t)(p->x * sx);
        p->y = (int16_t)(p->y * sy);
        if(p->x < path->min_x) path->min_x = p->x;
        if(p->x > path->max_x) path->max_x = p->x;
        if(p->y < path->min_y) path->min_y = p->y;
        if(p->y > path->max_y) path->max_y = p->y;
    }
    return true;
}

int16_t walk_reach(const WalkPath* path) {
    int16_t r = path->max_x;
    if(-path->min_x > r) r = -path->min_x;
    if(path->max_y > r) r = path->max_y;
    if(-path->min_y > r) r = -path->min_y;
    return r;
}
