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
#include "helper_protocol.h"
#include "link.h"
#include "util.h"

/* While something moves (a walk, the Remote's mouse, reports waiting), the
 * loop runs every walk step; otherwise ten times a second, which is enough
 * for the calm animations and lets the Flipper idle in between. */
#define BUSY_LOOP_MS WALK_STEP_MS
#define CALM_LOOP_MS 100

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

void duckwalk_sound(Duckwalk* app, Sound sound) {
    sounds_play(sound, app->settings.values[SettingSound], app->settings.values[SettingVibro]);
}

/* A lost connection is told once it has stayed lost for this long, so a
 * Bluetooth link that flickers at the edge of its range stays quiet. */
#define LINK_SETTLE_MS 3000

/* The desktop app reads the USB data channel every 50 ms. */
#define CLOSING_MS 150

/* Tells the user when the computer goes away and comes back, while Harold
 * is awake; asleep, nobody is counting on the link. */
static void check_link(Duckwalk* app) {
    bool now = link_is_connected();
    if(now != app->link_seen) {
        app->link_seen = now;
        app->link_seen_at = furi_get_tick();
    }
    if(now == app->link_told) return;
    if(!now && furi_get_tick() - app->link_seen_at < furi_ms_to_ticks(LINK_SETTLE_MS)) return;
    app->link_told = now;
    if(app->keep_awake.mode != KeepAwakeOff) duckwalk_sound(app, now ? SoundLinkBack : SoundLinkLost);
}

/* A command from the desktop app (an agent, say): as if pressed here. */
static void run_command(Duckwalk* app) {
    uint8_t cmd, arg;
    if(!helper_take_command(&cmd, &arg)) return;
    DuckwalkSettings* s = &app->settings;
    bool asleep = app->keep_awake.mode == KeepAwakeOff;
    app->active_at = furi_get_tick(); /* counts as somebody around */
    if(cmd == HELPER_CMD_WAKE && asleep) {
        s->values[SettingMode] = s->awake_mode;
        duckwalk_apply_settings(app);
        duckwalk_sound(app, SoundWake);
    } else if(cmd == HELPER_CMD_SLEEP && !asleep) {
        s->values[SettingMode] = KeepAwakeOff;
        duckwalk_apply_settings(app);
        duckwalk_sound(app, SoundSleep);
    } else if(cmd == HELPER_CMD_MODE) {
        s->values[SettingMode] = arg;
        duckwalk_apply_settings(app);
        app->pending.save = true;
        if(asleep) duckwalk_sound(app, SoundWake);
    }
    FURI_LOG_I(TAG, "Command %u from the desktop app", cmd);
}

/* What Harold is doing, for the desktop app (talk.h). */
static void tell_the_pc(Duckwalk* app) {
    const KeepAwake* ka = &app->keep_awake;
    TalkSeen seen = {
        .awake = ka->mode != KeepAwakeOff,
        .walking = keep_awake_is_walking(ka),
        .waiting = ka->user_active,
        .mode = (uint8_t)ka->mode,
        .connection = (uint8_t)link_kind(),
        .playing = keep_awake_is_playing(ka),
        .quack_until = app->home.quack_until,
    };
    talk_tick(&app->talk, &seen, &ka->path, furi_get_tick());
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
    sounds_init();
    app->screen = ScreenHome;
    app->running = true;
    app->active_at = furi_get_tick();
    /* Deadlines start now, not at 0: a Flipper up for over 24.8 days would
     * read a 0 as still to come. */
    app->home.quack_until = app->home.waddle_until = app->home.step_at = app->active_at;
    app->link_seen_at = app->active_at;
    app->remote.lit_until = app->trail.at = app->active_at;
    app->remote.page = RemotePageMenu;
    if(!settings_load(&app->settings)) app->screen = ScreenWelcome;
    keep_awake_init(&app->keep_awake, 0, 0);
    duckwalk_apply_settings(app);
    helper_start();
    talk_init(&app->talk);
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
        run_command(app);
        duckwalk_check_stop_after(app, got);
        check_link(app);
        if(furi_get_tick() - duty_checked >= furi_ms_to_ticks(DUTY_CHECK_MS)) {
            duty_checked = furi_get_tick();
            keep_awake_set_paused(&app->keep_awake, !settings_on_duty(&app->settings));
        }
        keep_awake_set_user_active(&app->keep_awake, helper_user_active(settings_resume_after_ms(&app->settings)));
        remote_tick(app);
        pairing_tick(app);
        if(keep_awake_tick(&app->keep_awake)) home_on_move(app);
        home_tick(app);
        preview_tick(app);
        tell_the_pc(app);
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

    /* Tell the desktop app, and give it a moment to read that before the
     * USB port goes back to the Flipper's own mode. */
    talk_closing();
    link_flush();
    furi_delay_ms(CLOSING_MS);
    helper_stop();
    link_stop();
    if(!app->skip_save) settings_save(&app->settings);
    FURI_LOG_I(TAG, "Stopped");

    furi_message_queue_free(queue);
    sounds_free();
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
