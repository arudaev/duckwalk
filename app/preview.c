/* Preview: the next walk drawn to scale, with a small Harold tracing it at
 * the speed it will play. OK plays it on the computer now, Right plans a
 * different one. While a walk is playing, Harold follows the real cursor. */

#include "duckwalk.h"
#include "duckwalk_icons.h"
#include "link.h"

#define BOX_X     6
#define BOX_Y     15
#define BOX_W     116
#define BOX_H     36
#define REST_STEPS 50 /* a second at the end before Harold starts over */

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

void preview_draw(Canvas* canvas, Duckwalk* app) {
    KeepAwake* ka = &app->keep_awake;
    const WalkPath* path = &ka->path;
    char line[24];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, keep_awake_is_walking(ka) ? "Walking" : "Next walk");
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
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, x - 5, y - 5, 10, 10);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_icon(canvas, x - 5, y - 5, &I_harold_mini_10x10);

    canvas_draw_str(canvas, 2, 63, link_is_connected() ? "OK walk now" : link_status());
    canvas_draw_str_aligned(canvas, 126, 63, AlignRight, AlignBottom, "new path >");
}

void preview_input(Duckwalk* app, const InputEvent* input) {
    if(input->type != InputTypeShort) return;
    KeepAwake* ka = &app->keep_awake;
    if(input->key == InputKeyOk) {
        keep_awake_walk_now(ka);
    } else if(input->key == InputKeyRight && !keep_awake_is_walking(ka)) {
        keep_awake_replan(ka);
        app->preview_started = furi_get_tick();
    } else if(input->key == InputKeyBack) {
        app->screen = ScreenHome;
    }
}
