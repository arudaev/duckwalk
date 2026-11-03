/* Pairing: a checklist that follows the connection from the Flipper to the
 * computer and ticks itself off, so the first open box shows which side
 * needs attention:
 *   1. Flipper – Harold is on air (Bluetooth on, profile running)
 *   2. PC      – the computer paired and connected
 *   3. PC      – the pointer really moves: Harold draws a small circle and
 *                asks whether you saw it
 * Right shows the fix for the first open box. Opens after choosing Bluetooth
 * in Settings or on the first launch, or with Down on the home screen. */

#include "duckwalk.h"
#include "duckwalk_icons.h"
#include "link.h"

#include <math.h>

#define DONE_MS      2000
#define CIRCLE_STEPS 40
#define CIRCLE_R     24.0f

typedef enum {
    StepOnAir,
    StepConnected,
    StepPointer,
    StepAllSet,
} Step;

static const struct {
    const char* label;
    const char* side;
} rows[] = {
    [StepOnAir] = {"Harold on air", "Flipper"},
    [StepConnected] = {"Paired, connected", "PC"},
    [StepPointer] = {"Pointer moves", "PC"},
};

static Step current_step(Duckwalk* app) {
    if(link_bt_off()) return StepOnAir;
    if(!link_is_connected()) return StepConnected;
    if(!app->pairing.pointer_ok) return StepPointer;
    return StepAllSet;
}

void pairing_open(Duckwalk* app) {
    memset(&app->pairing, 0, sizeof(app->pairing));
    app->screen = ScreenPairing;
}

static void draw_box(Canvas* canvas, int32_t x, int32_t y, bool done, bool failed, bool current) {
    bool blink = furi_get_tick() / furi_ms_to_ticks(500) % 2;
    if(current && !failed && blink) return;
    canvas_draw_frame(canvas, x, y, 8, 8);
    if(done) {
        canvas_draw_line(canvas, x + 1, y + 4, x + 3, y + 6);
        canvas_draw_line(canvas, x + 3, y + 6, x + 7, y + 1);
    } else if(failed) {
        canvas_draw_line(canvas, x + 2, y + 2, x + 5, y + 5);
        canvas_draw_line(canvas, x + 5, y + 2, x + 2, y + 5);
    }
}

static void draw_checklist(Canvas* canvas, Duckwalk* app) {
    Step step = current_step(app);
    char line[40];

    canvas_set_font(canvas, FontSecondary);
    for(Step i = StepOnAir; i <= StepPointer; i++) {
        int32_t y = 15 + i * 11;
        bool failed = i == StepPointer && app->pairing.pointer_failed && step == StepPointer;
        draw_box(canvas, 2, y, i < step, failed, i == step);
        canvas_draw_str(canvas, 14, y + 8, rows[i].label);
        canvas_draw_str_aligned(canvas, 126, y + 8, AlignRight, AlignBottom, rows[i].side);
    }

    /* What to do now, in one line. */
    const char* next = line;
    if(app->pairing.circle_step) {
        next = "Watch the pointer...";
    } else if(step == StepOnAir) {
        next = "Bluetooth is off: Help >";
    } else if(step == StepConnected) {
        snprintf(line, sizeof(line), "PC: add %s", link_bt_name());
    } else if(step == StepPointer && app->pairing.asked) {
        next = "Saw a circle? OK yes, < no";
    } else if(step == StepPointer && app->pairing.pointer_failed) {
        next = "PC ignores Harold: Help >";
    } else if(step == StepPointer) {
        next = "OK: test the pointer";
    } else {
        next = "All set!";
    }
    canvas_draw_line(canvas, 0, 50, 127, 50);
    canvas_draw_str(canvas, 2, 60, next);
}

static void draw_help(Canvas* canvas, Duckwalk* app) {
    Step step = current_step(app);
    canvas_set_font(canvas, FontSecondary);
    const char* lines[5] = {0};
    char pick[40];
    if(step == StepOnAir) {
        lines[0] = "On the Flipper: Settings";
        lines[1] = "> Bluetooth > ON.";
        lines[2] = "Close PC Health or";
        lines[3] = "Bluetooth Remote, then";
        lines[4] = "open Duckwalk again.";
    } else if(step == StepConnected) {
        snprintf(pick, sizeof(pick), "> pick %s", link_bt_name());
        lines[0] = "PC: Settings > Bluetooth";
        lines[1] = "> Add device > Bluetooth";
        lines[2] = pick;
        lines[3] = "Not listed? Hold OK here";
        lines[4] = "and remove Harold on PC.";
    } else {
        lines[0] = "Connected, but the PC";
        lines[1] = "ignores Harold. On the PC";
        lines[2] = "remove Harold, hold OK";
        lines[3] = "here, then add him again.";
    }
    for(size_t i = 0; i < 5 && lines[i]; i++) canvas_draw_str(canvas, 2, 21 + i * 9, lines[i]);
    canvas_draw_line(canvas, 0, 53, 127, 53);
    canvas_draw_str(canvas, 2, 63, "< Back");
    canvas_draw_str_aligned(canvas, 126, 63, AlignRight, AlignBottom, "hold OK reset");
}

void pairing_draw(Canvas* canvas, Duckwalk* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, app->pairing.help ? "Help" : "Bluetooth");
    canvas_draw_icon(canvas, 116, 1, &I_harold_mini_10x10);
    if(!link_is_connected()) {
        /* Radio waves from Harold while he waits for the computer. */
        uint32_t waves = furi_get_tick() / furi_ms_to_ticks(400) % 3;
        canvas_set_font(canvas, FontSecondary);
        for(uint32_t i = 0; i < waves; i++) canvas_draw_str(canvas, 111 - i * 4, 9, "(");
    }

    if(app->pairing.forgot_until && (int32_t)(app->pairing.forgot_until - furi_get_tick()) > 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 24, "Forgot every computer.");
        canvas_draw_str(canvas, 2, 34, "Now remove Harold on the");
        canvas_draw_str(canvas, 2, 44, "PC too, then add him");
        canvas_draw_str(canvas, 2, 54, "again.");
        return;
    }
    if(app->pairing.help) {
        draw_help(canvas, app);
    } else {
        draw_checklist(canvas, app);
    }
}

void pairing_input(Duckwalk* app, const InputEvent* input) {
    PairingState* p = &app->pairing;
    if(input->key == InputKeyOk && input->type == InputTypeLong) {
        link_bt_forget();
        memset(p, 0, sizeof(*p));
        p->forgot_until = furi_get_tick() + furi_ms_to_ticks(5000);
        return;
    }
    if(input->type != InputTypeShort) return;

    if(p->help) {
        if(input->key == InputKeyBack || input->key == InputKeyLeft) p->help = false;
        return;
    }
    Step step = current_step(app);
    switch(input->key) {
    case InputKeyBack:
        app->screen = ScreenHome;
        break;
    case InputKeyRight:
        if(step != StepAllSet) p->help = true;
        break;
    case InputKeyOk:
        if(step == StepPointer && p->asked) {
            p->pointer_ok = true;
            p->done_at = furi_get_tick();
        } else if(step == StepPointer && !p->circle_step) {
            p->circle_step = 1;
            p->circle_tick = furi_get_tick();
            p->asked = false;
            p->pointer_failed = false;
        }
        break;
    case InputKeyLeft:
        if(step == StepPointer && p->asked) {
            p->asked = false;
            p->pointer_failed = true;
        }
        break;
    default:
        break;
    }
}

static WalkPoint circle_point(uint8_t step) {
    float a = 2.0f * (float)M_PI * step / CIRCLE_STEPS;
    return (WalkPoint){(int16_t)lroundf(CIRCLE_R * sinf(a)), (int16_t)lroundf(CIRCLE_R * (1.0f - cosf(a)))};
}

void pairing_tick(Duckwalk* app) {
    PairingState* p = &app->pairing;
    if(app->screen != ScreenPairing) return;

    /* The pointer test: one small circle, one step every WALK_STEP_MS. */
    while(p->circle_step && (int32_t)(furi_get_tick() - p->circle_tick) >= 0) {
        WalkPoint from = circle_point(p->circle_step - 1), to = circle_point(p->circle_step);
        link_mouse_move(to.x - from.x, to.y - from.y);
        p->circle_tick += furi_ms_to_ticks(WALK_STEP_MS);
        if(++p->circle_step > CIRCLE_STEPS) {
            p->circle_step = 0;
            p->asked = true;
        }
    }

    if(!link_is_connected()) {
        p->pointer_ok = false;
        p->asked = false;
    }
    if(p->pointer_ok && furi_get_tick() - p->done_at > furi_ms_to_ticks(DONE_MS)) {
        app->screen = ScreenHome;
    }
}
