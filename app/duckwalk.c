/* Duckwalk: Harold keeps the computer awake and works as its remote.
 * This file owns the app's life: USB, the main loop and which screen is up.
 * Back on the home screen exits and gives the USB port back. */

#include <furi.h>
#include <furi_hal.h>

#include "duckwalk.h"
#include "usb/usb_duck.h"

/* Walks are played one step every WALK_STEP_MS, so the loop never sleeps
 * longer than that. */
#define LOOP_MS WALK_STEP_MS

/* How often the work-hours schedule is checked. */
#define DUTY_CHECK_MS 1000

static void draw_callback(Canvas* canvas, void* ctx) {
    Duckwalk* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(canvas);
    switch(app->screen) {
    case ScreenHome:
        home_draw(canvas, app);
        break;
    case ScreenSettings:
        settings_draw(canvas, app);
        break;
    case ScreenRemote:
        remote_draw(canvas, app);
        break;
    case ScreenPreview:
        preview_draw(canvas, app);
        break;
    }
    furi_mutex_release(app->mutex);
}

static void input_callback(InputEvent* input, void* ctx) {
    furi_message_queue_put(ctx, input, FuriWaitForever);
}

int32_t duckwalk_app(void* p) {
    UNUSED(p);
    Duckwalk* app = malloc(sizeof(Duckwalk));
    memset(app, 0, sizeof(Duckwalk));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->screen = ScreenHome;
    app->running = true;
    app->remote.page = RemotePageMenu;
    settings_load(&app->settings);
    keep_awake_init(&app->keep_awake, 0, 0);
    duckwalk_apply_settings(app);

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

    uint32_t duty_checked = 0;
    while(app->running) {
        InputEvent input;
        if(furi_message_queue_get(queue, &input, furi_ms_to_ticks(LOOP_MS)) == FuriStatusOk) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            switch(app->screen) {
            case ScreenHome:
                home_input(app, &input);
                break;
            case ScreenSettings:
                settings_input(app, &input);
                break;
            case ScreenRemote:
                remote_input(app, &input);
                break;
            case ScreenPreview:
                preview_input(app, &input);
                break;
            }
            furi_mutex_release(app->mutex);
        }

        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(furi_get_tick() - duty_checked >= furi_ms_to_ticks(DUTY_CHECK_MS)) {
            duty_checked = furi_get_tick();
            keep_awake_set_paused(&app->keep_awake, !settings_on_duty(&app->settings));
        }
        remote_tick(app);
        if(keep_awake_tick(&app->keep_awake)) home_on_move(app);
        furi_mutex_release(app->mutex);
        view_port_update(view_port);
    }

    if(app->usb_ok) furi_hal_usb_set_config(usb_prev, NULL);
    settings_save(&app->settings);

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(queue);
    furi_record_close(RECORD_NOTIFICATION);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
