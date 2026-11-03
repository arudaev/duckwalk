/* Duckwalk: Harold keeps the computer awake and works as its remote.
 * Home screen: Harold asleep (keep-awake off) or awake (on). OK toggles,
 * Back exits and gives the USB port back to the previous mode. */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>

#include "duckwalk_icons.h"
#include "keep_awake.h"
#include "usb/usb_duck.h"

/* Teams marks you away after a few minutes of idle time; nudging every
 * 40 to 110 seconds stays well inside that and inside common lock timers. */
#define SUBTLE_MIN_S 40
#define SUBTLE_MAX_S 110
#define WADDLE_MS    900

typedef struct {
    FuriMutex* mutex;
    bool usb_ok;
    KeepAwake keep_awake;
    uint32_t waddle_until;
} Duckwalk;

static const Icon* harold_frame(const Duckwalk* app) {
    if(app->keep_awake.mode == KeepAwakeOff) return &I_harold_asleep_28x29;
    if((int32_t)(app->waddle_until - furi_get_tick()) > 0) {
        return (furi_get_tick() / 150) % 2 ? &I_harold_walk_1_28x29 : &I_harold_walk_0_28x29;
    }
    return &I_harold_idle_28x29;
}

static void draw_callback(Canvas* canvas, void* ctx) {
    Duckwalk* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    char line[32];

    canvas_clear(canvas);
    canvas_draw_icon(canvas, 6, 20, harold_frame(app));
    if(app->keep_awake.mode == KeepAwakeOff) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 30, 20, "z");
        canvas_draw_str(canvas, 35, 15, "z");
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 44, 12, "Duckwalk");

    canvas_set_font(canvas, FontSecondary);
    const char* usb = !app->usb_ok              ? "USB busy" :
                      usb_duck_is_connected() ? "USB connected" :
                                                "USB waiting";
    canvas_draw_str(canvas, 44, 25, usb);

    if(app->keep_awake.mode == KeepAwakeOff) {
        canvas_draw_str(canvas, 44, 36, "Harold is asleep");
    } else {
        canvas_draw_str(canvas, 44, 36, "Keeping it awake");
        uint32_t s = keep_awake_seconds_left(&app->keep_awake);
        snprintf(line, sizeof(line), "next nudge %lu:%02lu", s / 60, s % 60);
        canvas_draw_str(canvas, 44, 47, line);
    }

    canvas_draw_line(canvas, 0, 53, 127, 53);
    canvas_draw_str_aligned(
        canvas,
        64,
        63,
        AlignCenter,
        AlignBottom,
        app->keep_awake.mode == KeepAwakeOff ? "OK wake Harold" : "OK let him sleep");

    furi_mutex_release(app->mutex);
}

static void input_callback(InputEvent* input, void* ctx) {
    furi_message_queue_put(ctx, input, FuriWaitForever);
}

int32_t duckwalk_app(void* p) {
    UNUSED(p);
    Duckwalk* app = malloc(sizeof(Duckwalk));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->waddle_until = 0;
    keep_awake_init(&app->keep_awake, SUBTLE_MIN_S, SUBTLE_MAX_S);
    keep_awake_set_mode(&app->keep_awake, KeepAwakeSubtle);

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, app);
    view_port_input_callback_set(view_port, input_callback, queue);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    FuriHalUsbInterface* usb_prev = furi_hal_usb_get_config();
    for(int i = 0; i < 20 && !(app->usb_ok = furi_hal_usb_set_config(&usb_duck, NULL)); i++) {
        furi_delay_ms(100);
    }

    bool running = true;
    while(running) {
        InputEvent input;
        if(furi_message_queue_get(queue, &input, 100) == FuriStatusOk && input.type == InputTypeShort) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            if(input.key == InputKeyBack) {
                running = false;
            } else if(input.key == InputKeyOk) {
                keep_awake_set_mode(
                    &app->keep_awake,
                    app->keep_awake.mode == KeepAwakeOff ? KeepAwakeSubtle : KeepAwakeOff);
            }
            furi_mutex_release(app->mutex);
        }

        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(keep_awake_tick(&app->keep_awake)) {
            app->waddle_until = furi_get_tick() + furi_ms_to_ticks(WADDLE_MS);
        }
        furi_mutex_release(app->mutex);
        view_port_update(view_port);
    }

    if(app->usb_ok) furi_hal_usb_set_config(usb_prev, NULL);

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(queue);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
