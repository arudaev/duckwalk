/* Connection check: a checklist that follows the link from the Flipper to
 * the computer and ticks itself off, so the first open box shows which side
 * needs attention. Over Bluetooth:
 *   1. Flipper – Harold is on air (Bluetooth on, profile running)
 *   2. PC      – the computer paired and connected
 *   3. PC      – the pointer really moves: Harold draws a small circle and
 *                asks whether you saw it
 * Over USB the same three, as: Harold has the USB port, the PC sees him, the
 * pointer moves. Right shows the fix for the first open box. Opens with Down
 * on the home screen, after choosing Bluetooth in Settings and on the first
 * launch. Once all is set it also says whether the PC helper is running. */

#include "duckwalk.h"
#include "duckwalk_icons.h"
#include "helper.h"
#include "link.h"
#include "util.h"

#define DONE_MS   2000
#define FORGOT_MS 5000

typedef enum {
    StepOnAir,
    StepConnected,
    StepPointer,
    StepAllSet,
} Step;

static const struct {
    const char* label;
    const char* side;
} rows[LinkCount][StepAllSet] = {
    [LinkUsb] =
        {
            [StepOnAir] = {"Harold has USB", "Flipper"},
            [StepConnected] = {"PC sees Harold", "PC"},
            [StepPointer] = {"Pointer moves", "PC"},
        },
    [LinkBluetooth] =
        {
            [StepOnAir] = {"Harold on air", "Flipper"},
            [StepConnected] = {"Paired, connected", "PC"},
            [StepPointer] = {"Pointer moves", "PC"},
        },
};

static Step current_step(Duckwalk* app) {
    if(link_off()) return StepOnAir;
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
    LinkKind kind = link_kind();
    bool usb = kind == LinkUsb;
    char line[40];

    canvas_set_font(canvas, FontSecondary);
    for(Step i = StepOnAir; i <= StepPointer; i++) {
        int32_t y = 15 + i * 11;
        bool failed = i == StepPointer && app->pairing.pointer_failed && step == StepPointer;
        draw_box(canvas, 2, y, i < step, failed, i == step);
        canvas_draw_str(canvas, 14, y + 8, rows[kind][i].label);
        canvas_draw_str_aligned(canvas, 126, y + 8, AlignRight, AlignBottom, rows[kind][i].side);
    }

    /* What to do now, in one line. */
    const char* next = line;
    if(app->pairing.testing) {
        next = "Watch the pointer...";
    } else if(step == StepOnAir) {
        next = usb ? "USB is busy: Help >" : "Not on air yet: Help >";
    } else if(step == StepConnected && usb) {
        next = "Plug into the PC: Help >";
    } else if(step == StepConnected) {
        snprintf(line, sizeof(line), "PC: add %s", link_bt_name());
    } else if(step == StepPointer && app->pairing.asked) {
        next = "Saw a circle? OK yes, < no";
    } else if(step == StepPointer && app->pairing.pointer_failed) {
        next = "PC ignores Harold: Help >";
    } else if(step == StepPointer) {
        next = "OK: test the pointer";
    } else {
        /* The helper is what makes Harold let go while you work. */
        next = helper_present() ? "All set, PC helper on" : "All set, no PC helper";
    }
    canvas_draw_line(canvas, 0, 50, 127, 50);
    canvas_draw_str(canvas, 2, 60, next);
}

static void draw_help(Canvas* canvas, Duckwalk* app) {
    Step step = current_step(app);
    bool usb = link_kind() == LinkUsb;
    canvas_set_font(canvas, FontSecondary);
    const char* lines[5] = {0};
    char pick[40];
    if(step == StepOnAir && usb) {
        lines[0] = "Another app holds the";
        lines[1] = "USB port. Close qFlipper";
        lines[2] = "and Flipper Lab on the";
        lines[3] = "PC, then open Duckwalk";
        lines[4] = "again.";
    } else if(step == StepConnected && usb) {
        lines[0] = "Plug the Flipper into the";
        lines[1] = "PC with a data cable.";
        lines[2] = "Charge-only cables don't";
        lines[3] = "work. Or pick Bluetooth";
        lines[4] = "in Settings.";
    } else if(step == StepPointer && usb) {
        lines[0] = "The PC sees Harold, but";
        lines[1] = "the pointer didn't move.";
        lines[2] = "Unplug the Flipper, plug";
        lines[3] = "it in again and retry.";
    } else if(step == StepOnAir) {
        lines[0] = "Harold couldn't start";
        lines[1] = "Bluetooth. Close apps";
        lines[2] = "that use it (PC Health,";
        lines[3] = "Bluetooth Remote) and";
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
    for(size_t i = 0; i < 5 && lines[i]; i++) canvas_draw_str(canvas, 2, 20 + i * 8, lines[i]);
    canvas_draw_line(canvas, 0, 53, 127, 53);
    canvas_draw_str(canvas, 2, 63, "< Back");
    if(!usb) canvas_draw_str_aligned(canvas, 126, 63, AlignRight, AlignBottom, "hold OK reset");
}

void pairing_draw(Canvas* canvas, Duckwalk* app) {
    canvas_set_font(canvas, FontPrimary);
    const char* title = link_kind() == LinkUsb ? "USB" : "Bluetooth";
    canvas_draw_str(canvas, 2, 10, app->pairing.help ? "Help" : title);
    canvas_draw_icon(canvas, 116, 1, &I_harold_mini_10x10);
    if(link_kind() == LinkBluetooth && !link_is_connected()) {
        /* Radio waves from Harold while he waits for the computer. */
        uint32_t waves = furi_get_tick() / furi_ms_to_ticks(400) % 3;
        canvas_set_font(canvas, FontSecondary);
        for(uint32_t i = 0; i < waves; i++) canvas_draw_str(canvas, 111 - i * 4, 9, "(");
    }

    if(app->pairing.forgot_until && tick_before(app->pairing.forgot_until)) {
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
        /* Forgetting paired computers only means something over Bluetooth. */
        if(link_kind() != LinkBluetooth) return;
        app->pending.forget = true;
        memset(p, 0, sizeof(*p));
        p->forgot_until = tick_in_ms(FORGOT_MS);
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
    case InputKeyUp: /* Down opened the check, so Up goes back */
        app->screen = ScreenHome;
        break;
    case InputKeyRight:
        if(step != StepAllSet) p->help = true;
        break;
    case InputKeyOk:
        if(step == StepPointer && p->asked) {
            p->pointer_ok = true;
            p->done_at = furi_get_tick();
        } else if(step == StepPointer && !p->testing && !keep_awake_is_playing(&app->keep_awake)) {
            keep_awake_play_circle(&app->keep_awake);
            p->testing = keep_awake_is_playing(&app->keep_awake);
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

void pairing_tick(Duckwalk* app) {
    PairingState* p = &app->pairing;
    if(app->screen != ScreenPairing) return;

    /* The pointer test: keep-awake plays the circle; ask once it's done. */
    if(p->testing && !keep_awake_is_playing(&app->keep_awake)) {
        p->testing = false;
        p->asked = true;
    }

    if(!link_is_connected()) {
        p->pointer_ok = false;
        p->asked = false;
    }
    if(p->pointer_ok && tick_reached(p->done_at + furi_ms_to_ticks(DONE_MS))) {
        app->screen = ScreenHome;
    }
}
