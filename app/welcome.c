/* Welcome: shown once, on the first launch. Harold says hello and asks how
 * to reach the computer; Bluetooth goes straight on to pairing. */

#include "duckwalk.h"
#include "duckwalk_icons.h"
#include "link.h"

void welcome_draw(Canvas* canvas, Duckwalk* app) {
    UNUSED(app);
    bool blink = furi_get_tick() % furi_ms_to_ticks(3000) < furi_ms_to_ticks(150);
    canvas_draw_icon(canvas, 2, 20, blink ? &I_harold_asleep_28x29 : &I_harold_idle_28x29);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 36, 12, "Hi, I'm Harold!");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 36, 24, "I keep your PC awake");
    canvas_draw_str(canvas, 36, 33, "while you're away.");
    canvas_draw_str(canvas, 36, 42, "How do I reach it?");

    canvas_draw_line(canvas, 0, 53, 127, 53);
    canvas_draw_str(canvas, 2, 63, "< USB cable");
    canvas_draw_str_aligned(canvas, 126, 63, AlignRight, AlignBottom, "Bluetooth >");
}

void welcome_input(Duckwalk* app, const InputEvent* input) {
    if(input->type != InputTypeShort) return;
    LinkKind kind;
    if(input->key == InputKeyBack) {
        /* Leaving without a choice: ask again next time. */
        app->skip_save = true;
        app->running = false;
        return;
    } else if(input->key == InputKeyLeft) {
        kind = LinkUsb;
    } else if(input->key == InputKeyRight) {
        kind = LinkBluetooth;
    } else {
        return;
    }
    app->settings.values[SettingConnection] = kind;
    duckwalk_apply_settings(app);
    app->pending.save = true;
    if(kind == LinkBluetooth) {
        pairing_open(app);
    } else {
        app->screen = ScreenHome;
    }
}
