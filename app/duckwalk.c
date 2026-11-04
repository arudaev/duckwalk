/* Duckwalk: Harold keeps the computer awake and works as its remote.
 * This file owns the app's life: the link to the computer, the main loop and
 * which screen is up. Back on the home screen exits and gives the USB port
 * or the Bluetooth profile back.
 *
 * Threads and the app lock (app->mutex):
 *   - main thread (this loop): input, ticks and all state changes, under the
 *     lock; then, without it, the slow work: sending queued HID reports,
 *     changing the connection, saving settings
 *   - GUI thread: draw_callback, which only reads state under the lock and
 *     never waits for it for long
 *   - input service: input_callback, which only queues the event
 *   - Bt service and USB interrupt: link.c and usb/usb_duck.c, which only
 *     store a status the other threads read */

#include <furi.h>
#include <furi_hal.h>

#include "duckwalk.h"
#include "helper.h"
#include "link.h"
#include "util.h"

/* While something moves (a walk, the Remote's mouse, reports waiting), the
 * loop runs every walk step; otherwise ten times a second, which is enough
 * for the calm animations and lets the Flipper idle in between. */
#define BUSY_LOOP_MS WALK_STEP_MS
#define CALM_LOOP_MS 100

/* With the PC helper running, Harold keeps his hands off the pointer for
 * this long after the user's last own input. */
#define USER_HOLD_MS 30000

/* How often the work-hours schedule is checked. */
#define DUTY_CHECK_MS 1000

/* The GUI thread must never wait on the app for long: the firmware's
 * Bluetooth pairing dialog can only appear while the GUI keeps running. The
 * main loop holds the lock only for quick state changes, so this is ample;
 * if it ever times out, one frame is skipped. */
#define DRAW_WAIT_MS 25

#define INPUT_QUEUE_LEN 16

/* The longest Harold takes to bring the pointer home when you leave. */
#define LET_GO_MS (WANDER_HOME_STEPS * WALK_STEP_MS + 500)

static void draw_callback(Canvas* canvas, void* ctx) {
    Duckwalk* app = ctx;
    if(furi_mutex_acquire(app->mutex, furi_ms_to_ticks(DRAW_WAIT_MS)) != FuriStatusOk) return;
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
    case ScreenPairing:
        pairing_draw(canvas, app);
        break;
    case ScreenWelcome:
        welcome_draw(canvas, app);
        break;
    }
    furi_mutex_release(app->mutex);
}

static void input_callback(InputEvent* input, void* ctx) {
    /* Never block the input service; a full queue drops the press. */
    furi_message_queue_put(ctx, input, 0);
}

static void handle_input(Duckwalk* app, const InputEvent* input) {
    switch(app->screen) {
    case ScreenHome:
        home_input(app, input);
        break;
    case ScreenSettings:
        settings_input(app, input);
        break;
    case ScreenRemote:
        remote_input(app, input);
        break;
    case ScreenPreview:
        preview_input(app, input);
        break;
    case ScreenPairing:
        pairing_input(app, input);
        break;
    case ScreenWelcome:
        welcome_input(app, input);
        break;
    }
}

/* Slow work the screens asked for, done without the app lock. */
static void run_pending(Duckwalk* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    DuckwalkPending pending = app->pending;
    DuckwalkSettings settings = app->settings;
    memset(&app->pending, 0, sizeof(app->pending));
    furi_mutex_release(app->mutex);

    if(pending.link) link_start(settings.values[SettingConnection]);
    if(pending.forget) link_bt_forget();
    if(pending.save) settings_save(&settings);
    link_flush();
}

int32_t duckwalk_app(void* p) {
    UNUSED(p);
    Duckwalk* app = malloc(sizeof(Duckwalk));
    memset(app, 0, sizeof(Duckwalk));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->screen = ScreenHome;
    app->running = true;
    app->active_at = furi_get_tick();
    /* Deadlines start now, not at 0: a Flipper up for over 24.8 days would
     * read a 0 as still to come. */
    app->home.quack_until = app->home.waddle_until = app->active_at;
    app->remote.lit_until = app->trail.at = app->active_at;
    app->remote.page = RemotePageMenu;
    if(!settings_load(&app->settings)) app->screen = ScreenWelcome;
    keep_awake_init(&app->keep_awake, 0, 0);
    duckwalk_apply_settings(app);
    helper_start();
    FURI_LOG_I(TAG, "Started, %s", app->screen == ScreenWelcome ? "first launch" : "settings loaded");

    FuriMessageQueue* queue = furi_message_queue_alloc(INPUT_QUEUE_LEN, sizeof(InputEvent));
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, app);
    view_port_input_callback_set(view_port, input_callback, queue);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    uint32_t duty_checked = 0;
    bool busy = true;
    while(app->running) {
        InputEvent input;
        uint32_t wait = furi_ms_to_ticks(busy ? BUSY_LOOP_MS : CALM_LOOP_MS);
        bool got = furi_message_queue_get(queue, &input, wait) == FuriStatusOk;

        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(got) handle_input(app, &input);
        duckwalk_check_stop_after(app, got);
        if(furi_get_tick() - duty_checked >= furi_ms_to_ticks(DUTY_CHECK_MS)) {
            duty_checked = furi_get_tick();
            keep_awake_set_paused(&app->keep_awake, !settings_on_duty(&app->settings));
        }
        keep_awake_set_user_active(&app->keep_awake, helper_user_active(USER_HOLD_MS));
        remote_tick(app);
        pairing_tick(app);
        if(keep_awake_tick(&app->keep_awake)) home_on_move(app);
        home_tick(app);
        preview_tick(app);
        busy = keep_awake_is_walking(&app->keep_awake) || app->remote.held || link_busy();
        furi_mutex_release(app->mutex);

        run_pending(app);
        view_port_update(view_port);
    }

    /* A wander glides the pointer home before Harold lets go of it. */
    uint32_t leaving = furi_get_tick();
    while(furi_get_tick() - leaving < furi_ms_to_ticks(LET_GO_MS)) {
        /* The screen is still up and reads this state as it draws. */
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        bool done = keep_awake_let_go(&app->keep_awake);
        if(!done) keep_awake_tick(&app->keep_awake);
        furi_mutex_release(app->mutex);
        if(done) break;
        link_flush();
        furi_delay_ms(WALK_STEP_MS);
    }

    /* The screen goes first, so nothing draws while the link is torn down. */
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);

    helper_stop();
    link_stop();
    if(!app->skip_save) settings_save(&app->settings);
    FURI_LOG_I(TAG, "Stopped");

    furi_message_queue_free(queue);
    furi_record_close(RECORD_NOTIFICATION);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
