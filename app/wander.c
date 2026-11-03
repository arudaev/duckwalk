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

/* A new spot somewhere in range, far enough from here to be worth the trip,
 * and maybe a new pace to get there. */
static void pick_target(Wander* w) {
    for(int tries = 0; tries < 8; tries++) {
        float a = random_float(0, 2.0f * (float)M_PI);
        /* sqrt spreads the spots evenly over the disc, not bunched in the middle */
        float r = sqrtf(random_float(0, 1)) * w->range;
        w->tx = cosf(a) * r;
        w->ty = sinf(a) * r;
        if(hypotf(w->tx - w->x, w->ty - w->y) > w->range * 0.35f) break;
    }
    if(furi_hal_random_get() % 3 == 0) {
        w->cruise = cruise_speeds[furi_hal_random_get() % COUNT_OF(cruise_speeds)];
    }
    /* Its turning circle has to fit the range, or it swings far out on every
     * turn: a short wander ambles. */
    float fastest = TURN_PER_STEP * w->range / 4.0f;
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
    w->target_steps = 0;
    pick_target(w);
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
