/* Preview: the next walk drawn to scale, with a small Harold tracing it at
 * the speed it will play. OK plays it on the computer now, Right plans a
 * different one. While a walk is playing, Harold follows the real cursor.
 * In Wander mode the path is a sample of wandering; while Harold wanders,
 * he is shown live on the area he wanders over. */

#include "duckwalk.h"
#include "duckwalk_icons.h"
#include "link.h"
#include "util.h"

#define BOX_X     6
#define BOX_Y     15
#define BOX_W     116
#define BOX_H     36
#define REST_STEPS 50 /* a second at the end before Harold starts over */
#define TRAIL_MS   100 /* a point of the wander's trail every so often */

typedef struct {
    float scale;
    int32_t ox;
    int32_t oy;
} Fit;

static Fit fit_path(const WalkPath* path) {
    int32_t w = path->max_x - path->min_x, h = path->max_y - path->min_y;
    Fit fit;
    float sx = w ? (float)BOX_W / w : 1.0f, sy = h ? (float)BOX_H / h : 1.0f;
    fit.scale = sx < sy ? sx : sy;
    if(fit.scale > 1.0f) fit.scale = 1.0f;
    fit.ox = BOX_X + (BOX_W - (int32_t)(w * fit.scale)) / 2 - (int32_t)(path->min_x * fit.scale);
    fit.oy = BOX_Y + (BOX_H - (int32_t)(h * fit.scale)) / 2 - (int32_t)(path->min_y * fit.scale);
    return fit;
}

static void to_screen(const Fit* fit, WalkPoint p, int32_t* x, int32_t* y) {
    *x = fit->ox + (int32_t)(p.x * fit->scale);
    *y = fit->oy + (int32_t)(p.y * fit->scale);
}

static void draw_harold(Canvas* canvas, int32_t x, int32_t y) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, x - 5, y - 5, 10, 10);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_icon(canvas, x - 5, y - 5, &I_harold_mini_10x10);
}

/* Live: the area Harold wanders over, and where he is in it. */
static void draw_wandering(Canvas* canvas, KeepAwake* ka, const Trail* trail) {
    char line[24];
    int16_t reach = keep_awake_reach(ka);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Wandering");
    canvas_set_font(canvas, FontSecondary);
    snprintf(line, sizeof(line), "%d px around", reach);
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, line);

    WalkPath box = {.min_x = -reach, .max_x = reach, .min_y = -reach, .max_y = reach};
    Fit fit = fit_path(&box);
    int32_t x, y;
    to_screen(&fit, (WalkPoint){0, 0}, &x, &y);
    canvas_draw_circle(canvas, x, y, 2);
    /* The trail: solid near Harold, every other dot further back. */
    for(uint8_t i = 0; i < trail->count; i++) {
        uint8_t age = trail->count - 1 - i;
        if(age > TRAIL_LEN / 2 && age % 2) continue;
        to_screen(&fit, trail->points[(trail->head + TRAIL_LEN - 1 - age) % TRAIL_LEN], &x, &y);
        canvas_draw_dot(canvas, x, y);
    }
    to_screen(&fit, keep_awake_position(ka), &x, &y);
    draw_harold(canvas, x, y);
    canvas_draw_str(canvas, 2, 63, "Never stops");
}

void preview_draw(Canvas* canvas, Duckwalk* app) {
    KeepAwake* ka = &app->keep_awake;
    if(keep_awake_is_wandering(ka) && !keep_awake_is_playing(ka)) {
        draw_wandering(canvas, ka, &app->trail);
        return;
    }
    const WalkPath* path = &ka->path;
    char line[24];

    canvas_set_font(canvas, FontPrimary);
    const char* title = ka->mode == KeepAwakeWander ? "Wander" : "Next walk";
    canvas_draw_str(canvas, 2, 10, keep_awake_is_walking(ka) ? "Walking" : title);
    canvas_set_font(canvas, FontSecondary);
    uint32_t tenths = path->count * WALK_STEP_MS / 100;
    snprintf(line, sizeof(line), "%lu.%lu s, %d px", tenths / 10, tenths % 10, walk_reach(path));
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, line);

    Fit fit = fit_path(path);
    int32_t px, py, x, y;
    to_screen(&fit, path->points[0], &px, &py);
    canvas_draw_circle(canvas, px, py, 2);
    for(uint16_t i = 1; i < path->count; i++) {
        to_screen(&fit, path->points[i], &x, &y);
        if(x != px || y != py) canvas_draw_line(canvas, px, py, x, y);
        px = x;
        py = y;
    }

    WalkPoint at;
    if(keep_awake_is_walking(ka)) {
        at = keep_awake_position(ka);
    } else {
        uint32_t step = (furi_get_tick() - app->preview_started) / furi_ms_to_ticks(WALK_STEP_MS);
        step %= path->count + REST_STEPS;
        if(step >= path->count) step = path->count - 1;
        at = path->points[step];
    }
    to_screen(&fit, at, &x, &y);
    draw_harold(canvas, x, y);

    canvas_draw_str(canvas, 2, 63, link_is_connected() ? "OK walk now" : link_status());
    canvas_draw_str_aligned(canvas, 126, 63, AlignRight, AlignBottom, "new path >");
}

void preview_input(Duckwalk* app, const InputEvent* input) {
    if(input->type != InputTypeShort) return;
    KeepAwake* ka = &app->keep_awake;
    if(input->key == InputKeyOk) {
        keep_awake_walk_now(ka);
    } else if(input->key == InputKeyRight && !keep_awake_is_playing(ka)) {
        keep_awake_replan(ka);
        app->preview_started = furi_get_tick();
    } else if(input->key == InputKeyBack) {
        app->screen = ScreenHome;
    }
}

void preview_tick(Duckwalk* app) {
    Trail* trail = &app->trail;
    if(!keep_awake_is_wandering(&app->keep_awake)) {
        trail->count = 0;
        return;
    }
    if(!tick_reached(trail->at)) return;
    trail->at = tick_in_ms(TRAIL_MS);
    trail->points[trail->head] = keep_awake_position(&app->keep_awake);
    trail->head = (trail->head + 1) % TRAIL_LEN;
    if(trail->count < TRAIL_LEN) trail->count++;
}
