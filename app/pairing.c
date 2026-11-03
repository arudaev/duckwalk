/* Pairing: how to add Harold on the computer, with the exact name to pick.
 * Opens after choosing Bluetooth in Settings, or with Down on the home
 * screen. Returns home on its own once the computer connects. Holding OK
 * forgets every paired computer. */

#include "duckwalk.h"
#include "duckwalk_icons.h"
#include "link.h"

#define CONNECTED_MS 2000
#define FORGOT_MS    4000

static bool before(uint32_t until) {
    return (int32_t)(until - furi_get_tick()) > 0;
}

void pairing_open(Duckwalk* app) {
    app->pairing.connected_at = 0;
    app->pairing.forgot_until = 0;
    app->screen = ScreenPairing;
}

void pairing_draw(Canvas* canvas, Duckwalk* app) {
    uint32_t now = furi_get_tick();
    bool connected = link_is_connected();

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Bluetooth");
    canvas_draw_icon(canvas, 116, 1, &I_harold_mini_10x10);
    if(!connected) {
        /* Radio waves from Harold while he advertises. */
        uint32_t waves = now / furi_ms_to_ticks(400) % 3;
        canvas_set_font(canvas, FontSecondary);
        for(uint32_t i = 0; i < waves; i++) canvas_draw_str(canvas, 111 - i * 4, 9, "(");
    }

    canvas_set_font(canvas, FontSecondary);
    if(connected) {
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignBottom, "Connected to");
    } else {
        canvas_draw_str(canvas, 2, 21, "On the computer, open");
        canvas_draw_str(canvas, 2, 30, "Bluetooth > Add device:");
    }
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignBottom, link_bt_name());

    canvas_set_font(canvas, FontSecondary);
    if(before(app->pairing.forgot_until)) {
        canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignBottom, "Forgot. Remove on PC too");
    } else if(!connected) {
        const char* waiting[] = {"Waiting", "Waiting.", "Waiting..", "Waiting..."};
        canvas_draw_str_aligned(
            canvas, 64, 52, AlignCenter, AlignBottom, waiting[now / furi_ms_to_ticks(400) % 4]);
    }

    canvas_draw_str(canvas, 2, 63, "Back later");
    canvas_draw_str_aligned(canvas, 126, 63, AlignRight, AlignBottom, "hold OK forget");
}

void pairing_input(Duckwalk* app, const InputEvent* input) {
    if(input->key == InputKeyBack && input->type == InputTypeShort) {
        app->screen = ScreenHome;
    } else if(input->key == InputKeyOk && input->type == InputTypeLong) {
        link_bt_forget();
        app->pairing.connected_at = 0;
        app->pairing.forgot_until = furi_get_tick() + furi_ms_to_ticks(FORGOT_MS);
    }
}

void pairing_tick(Duckwalk* app) {
    if(app->screen != ScreenPairing) return;
    if(!link_is_connected()) {
        app->pairing.connected_at = 0;
        return;
    }
    if(!app->pairing.connected_at) app->pairing.connected_at = furi_get_tick();
    if(furi_get_tick() - app->pairing.connected_at > furi_ms_to_ticks(CONNECTED_MS)) {
        app->screen = ScreenHome;
    }
}
