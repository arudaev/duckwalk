#include "wander.h"

#include <furi_hal_random.h>
#include <math.h>

/* How far it strays, in mouse counts: the same reach as a walk of that
 * length. */
static const float wander_ranges[WalkLengthCount] = {
    [WalkLengthShort] = 60.0f,
    [WalkLengthMedium] = 140.0f,
    [WalkLengthLong] = 280.0f,
};

/* Speeds in counts per 20 ms step: an amble, a walk and a brisk walk (80,
 * 120 and 160 counts a second). Kept close together, and home at the middle
 * one: with Windows' "Enhance pointer precision" on (the default), faster
 * moves go further per count, so a fast way home would overshoot the slow
 * way out. */
static const float cruise_speeds[] = {1.6f, 2.4f, 3.2f};
#define HOME_SPEED 2.4f

#define TURN_PER_STEP 0.10f /* radians: how sharply it can turn */
#define EASE_SPEED    0.04f /* how quickly the speed follows the cruise */
#define NEAR_TARGET   30.0f /* close enough: pick the next spot */
#define TARGET_STEPS  150 /* give up on a spot after three seconds */
#define SLOW_DOWN     40.0f /* homing: start braking this close */

static float random_float(float lo, float hi) {
    return lo + (hi - lo) * (float)(furi_hal_random_get() % 10001) / 10000.0f;
}

static float wrap_angle(float a) {
    while(a > (float)M_PI) a -= 2.0f * (float)M_PI;
    while(a < -(float)M_PI) a += 2.0f * (float)M_PI;
    return a;
}

/* Is (x, y) where spots are picked? */
static bool in_spots(const Wander* w, float x, float y) {
    return !w->has_area || (x >= w->x0 && x <= w->x1 && y >= w->y0 && y <= w->y1);
}

/* Along one axis: where spots may be, given the area [lo, hi). The reach
 * [-r, r] moves towards the area, a quarter of a reach at most (so the way
 * home still fits WANDER_HOME_STEPS), until they overlap enough to wander
 * in: half a reach, or the whole area if it is smaller. Then spots are
 * picked in the overlap. Too far for that, they are picked in the reach on
 * the area's side of the start, so the wander heads that way. Returns true
 * when the spots are inside the area. */
static bool fit_axis(float lo, float hi, float r, float* from, float* to) {
    float need = fminf(r / 2, hi - 1 - lo);
    float dir = lo + hi > 0 ? 1.0f : -1.0f;
    for(int k = 0; k <= 2; k++) {
        float s = dir * k * r / 8;
        *from = fmaxf(lo, -r + s);
        *to = fminf(hi - 1, r + s);
        if(*to - *from >= need) return true;
    }
    *from = dir > 0 ? 0 : -r;
    *to = dir > 0 ? r : 0;
    return false;
}

/* Where spots may be, from the area: computed when the area or the reach
 * changes. A wander strays little further than it would without one, so
 * the way home stays short. */
static void check_reach(Wander* w) {
    if(!w->has_area) {
        w->bounded = false;
        return;
    }
    bool in_x = fit_axis(w->left, w->right, w->range, &w->x0, &w->x1);
    bool in_y = fit_axis(w->top, w->bottom, w->range, &w->y0, &w->y1);
    w->bounded = in_x && in_y;
}

static void spot_in_area(Wander* w) {
    w->tx = random_float(w->x0, w->x1);
    w->ty = random_float(w->y0, w->y1);
}

/* A new spot somewhere in range, far enough from here to be worth the trip,
 * and maybe a new pace to get there. */
static void pick_target(Wander* w) {
    for(int tries = 0; tries < 8; tries++) {
        if(w->has_area) {
            spot_in_area(w);
        } else {
            float a = random_float(0, 2.0f * (float)M_PI);
            /* sqrt spreads the spots evenly over the disc, not bunched in the middle */
            float r = sqrtf(random_float(0, 1)) * w->range;
            w->tx = cosf(a) * r;
            w->ty = sinf(a) * r;
        }
        if(hypotf(w->tx - w->x, w->ty - w->y) > w->range * 0.35f) break;
    }
    if(furi_hal_random_get() % 3 == 0) {
        w->cruise = cruise_speeds[furi_hal_random_get() % COUNT_OF(cruise_speeds)];
    }
    /* Its turning circle has to fit the range and the area, or it swings far
     * out on every turn: a short wander, or one in a small window, ambles. */
    float room = w->range;
    if(w->has_area) room = fminf(room, fminf(w->x1 - w->x0, w->y1 - w->y0) / 2.0f);
    float fastest = fmaxf(TURN_PER_STEP * room / 4.0f, 0.8f);
    if(w->cruise > fastest) w->cruise = fastest;
    w->target_steps = 0;
}

void wander_start(Wander* w, WalkLength length) {
    furi_check(length < WalkLengthCount);
    w->x = w->y = 0;
    w->sent_x = w->sent_y = 0;
    w->range = wander_ranges[length];
    w->heading = random_float(0, 2.0f * (float)M_PI);
    w->speed = 0;
    w->cruise = cruise_speeds[1];
    w->homing = false;
    w->has_area = false;
    w->bounded = false;
    w->target_steps = 0;
    pick_target(w);
}

void wander_set_area(Wander* w, int16_t left, int16_t top, int16_t right, int16_t bottom) {
    if(left >= right || top >= bottom) return;
    w->has_area = true;
    w->left = left;
    w->top = top;
    w->right = right;
    w->bottom = bottom;
    check_reach(w);
    if(!w->homing && !in_spots(w, w->tx, w->ty)) pick_target(w);
}

void wander_shift(Wander* w, int16_t dx, int16_t dy) {
    if(!dx && !dy) return;
    w->x += dx;
    w->y += dy;
    w->sent_x += dx;
    w->sent_y += dy;
    if(w->homing && w->speed < 0.6f) {
        /* Stopped at home, or nearly: the slowest homing pace, straight back. */
        w->speed = 0.6f;
        w->heading = atan2f(-w->y, -w->x);
    }
}

void wander_set_length(Wander* w, WalkLength length) {
    furi_check(length < WalkLengthCount);
    w->range = wander_ranges[length];
    check_reach(w);
    /* Head somewhere in the new reach now, at a pace that fits it. */
    if(!w->homing) pick_target(w);
}

void wander_go_home(Wander* w) {
    if(w->homing) return;
    w->homing = true;
    w->home_steps = 0;
    w->tx = w->ty = 0;
    w->cruise = HOME_SPEED;
}

static void steer(Wander* w) {
    float dx = w->tx - w->x, dy = w->ty - w->y;
    float dist = hypotf(dx, dy);

    if(w->homing) {
        if(dist <= w->speed || dist < 0.5f) {
            w->x = w->y = 0;
            w->speed = 0;
            return;
        }
        w->home_steps++;
    } else if(dist < NEAR_TARGET || ++w->target_steps > TARGET_STEPS) {
        pick_target(w);
        dx = w->tx - w->x;
        dy = w->ty - w->y;
    }

    float pace = w->cruise;
    if(w->homing) {
        /* Its turning circle (speed / turn) has to fit inside the distance
         * left, or it could circle the start for ever; near the end it
         * brakes. */
        float fits = dist * TURN_PER_STEP / 2.0f, brake = dist * 0.15f;
        if(fits < pace) pace = fits;
        if(brake < pace) pace = brake;
        if(pace < 0.6f) pace = 0.6f;
    }

    /* Turn towards the spot, but only so far each step: smooth arcs. The
     * last stretch home, or a homing that takes too long, goes straight. */
    float turn = wrap_angle(atan2f(dy, dx) - w->heading);
    bool straight = w->homing && (dist < SLOW_DOWN || w->home_steps > WANDER_HOME_STEPS - 60);
    if(!straight && turn > TURN_PER_STEP) turn = TURN_PER_STEP;
    if(!straight && turn < -TURN_PER_STEP) turn = -TURN_PER_STEP;
    w->heading = wrap_angle(w->heading + turn);

    w->speed += (pace - w->speed) * EASE_SPEED;
    if(w->homing && w->speed > pace) w->speed = pace;
    w->x += cosf(w->heading) * w->speed;
    w->y += sinf(w->heading) * w->speed;
}

WalkPoint wander_step(Wander* w) {
    steer(w);
    int16_t x = (int16_t)lroundf(w->x), y = (int16_t)lroundf(w->y);
    WalkPoint move = {x - w->sent_x, y - w->sent_y};
    w->sent_x = x;
    w->sent_y = y;
    return move;
}

bool wander_is_home(const Wander* w) {
    return w->homing && w->sent_x == 0 && w->sent_y == 0 && w->x == 0 && w->y == 0;
}

WalkPoint wander_position(const Wander* w) {
    return (WalkPoint){w->sent_x, w->sent_y};
}

int16_t wander_reach(const Wander* w) {
    return (int16_t)w->range;
}

void wander_sample(WalkPath* path, WalkLength length) {
    Wander w;
    wander_start(&w, length);
    path->count = 0;
    path->min_x = path->max_x = path->min_y = path->max_y = 0;
    path->points[path->count++] = (WalkPoint){0, 0};
    while(path->count < WALK_MAX_STEPS) {
        if(path->count == WALK_MAX_STEPS - WANDER_HOME_STEPS - 1) wander_go_home(&w);
        wander_step(&w);
        WalkPoint p = wander_position(&w);
        path->points[path->count++] = p;
        if(p.x < path->min_x) path->min_x = p.x;
        if(p.x > path->max_x) path->max_x = p.x;
        if(p.y < path->min_y) path->min_y = p.y;
        if(p.y > path->max_y) path->max_y = p.y;
        if(wander_is_home(&w)) break;
    }
    /* Never leave the cursor off its start, even if home took too long. */
    path->points[path->count - 1] = (WalkPoint){0, 0};
}
