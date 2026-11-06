/* Draws every Duckwalk screen in the states that matter and saves each as a
 * PNG, at the Flipper's 128x64 and four times larger, in the display's
 * orange and black. Exits non-zero if any text runs off the screen or a
 * scene doesn't reach the state it sets up.
 *
 *   duckwalk_sim <output folder>
 */

#include "duckwalk.h"
#include "helper.h"
#include "helper_protocol.h"
#include "sim.h"

#include <furi_hal_random.h>
#include "png.h"

#define SCALE 4
#define NOW   100000 /* any tick well past zero */

static const uint8_t lcd[3] = {0xFF, 0x82, 0x00}; /* backlight */
static const uint8_t ink[3] = {0x00, 0x00, 0x00};

static Duckwalk app;

/* A fresh app, as after launch: Walk mode, USB connected, nothing pressed. */
static void fresh(void) {
    memset(&app, 0, sizeof(app));
    shim_tick_set(NOW);
    shim_random_seed(7);
    settings_defaults(&app.settings);
    keep_awake_init(&app.keep_awake, 0, 0);
    duckwalk_apply_settings(&app);
    app.screen = ScreenHome;
    app.remote.page = RemotePageMenu;
    app.home.blink_at = NOW + 5000;
    app.active_at = NOW;
    sim_link = (SimLink){.kind = LinkUsb, .connected = true, .on_air = true};
    helper_stop(); /* no PC helper unless a scene says so */
}

/* A scene that didn't reach its state fails the run, like text off the
 * screen does. */
static int unmet = 0;
static void expect(bool ok, const char* what) {
    if(ok) return;
    fprintf(stderr, "%s\n", what);
    unmet++;
}

/* Let time pass the way the main loop would. */
static void run_for(uint32_t ms) {
    for(uint32_t t = 0; t < ms; t += WALK_STEP_MS) {
        shim_tick_set(furi_get_tick() + WALK_STEP_MS);
        keep_awake_set_user_active(&app.keep_awake, helper_user_active(30000));
        remote_tick(&app);
        pairing_tick(&app);
        preview_tick(&app);
        duckwalk_check_stop_after(&app, false);
        if(keep_awake_tick(&app.keep_awake)) home_on_move(&app);
        home_tick(&app);
    }
}

static void press(InputKey key, InputType type) {
    InputEvent event = {.key = key, .type = type};
    switch(app.screen) {
    case ScreenHome:
        home_input(&app, &event);
        break;
    case ScreenSettings:
        settings_input(&app, &event);
        break;
    case ScreenRemote:
        remote_input(&app, &event);
        break;
    case ScreenPreview:
        preview_input(&app, &event);
        break;
    case ScreenPairing:
        pairing_input(&app, &event);
        break;
    case ScreenWelcome:
        welcome_input(&app, &event);
        break;
    }
}

static void draw(Canvas* canvas) {
    switch(app.screen) {
    case ScreenHome:
        home_draw(canvas, &app);
        break;
    case ScreenSettings:
        settings_draw(canvas, &app);
        break;
    case ScreenRemote:
        remote_draw(canvas, &app);
        break;
    case ScreenPreview:
        preview_draw(canvas, &app);
        break;
    case ScreenPairing:
        pairing_draw(canvas, &app);
        break;
    case ScreenWelcome:
        welcome_draw(canvas, &app);
        break;
    }
}

/* Scenes: each starts from fresh() and gets the app into one state. */

static void home_awake(void) {
    run_for(3000);
}
static void home_bug(void) {
    run_for(1000);
    app.home.bug_at = furi_get_tick() + 100;
    run_for(600);
}
static void home_bug_quack(void) {
    home_bug();
    run_for(1200);
}
static void home_bug_fixed(void) {
    home_bug_quack();
    run_for(900);
}
/* Start one of Harold's routines and catch it partway through. */
static void routine(Routine r, uint32_t ms) {
    run_for(500);
    app.home.routine = r;
    app.home.routine_start = furi_get_tick();
    app.home.routine_until = furi_get_tick() + 3000;
    run_for(ms);
}
static void home_sip(void) {
    routine(RoutineSip, 600);
}
static void home_typing(void) {
    routine(RoutineType, 300);
}
static void home_hop(void) {
    routine(RoutineHop, 20);
}
static void home_asleep(void) {
    press(InputKeyOk, InputTypeShort);
    run_for(600);
}
static void home_stopped(void) {
    /* Stop after ran out: nobody pressed anything for the default 4 h,
     * long enough to catch a 32-bit overflow in the idle time. */
    run_for(500);
    app.active_at = furi_get_tick() - 4u * 3600u * 1000u;
    run_for(500);
    expect(app.stopped, "home-stopped: Stop after didn't fire");
}
static void home_walking(void) {
    keep_awake_walk_now(&app.keep_awake);
    run_for(1300);
}
static void wander_mode(void) {
    app.settings.values[SettingMode] = KeepAwakeWander;
    duckwalk_apply_settings(&app);
}
static void home_wandering(void) {
    wander_mode();
    run_for(2500);
}
static void home_wander_homing(void) {
    home_wandering();
    app.settings.values[SettingMode] = KeepAwakeOff;
    duckwalk_apply_settings(&app);
    run_for(200);
}
static void preview_wandering(void) {
    home_wandering();
    press(InputKeyUp, InputTypeShort);
    run_for(500);
}
static void home_quack(void) {
    press(InputKeyOk, InputTypeLong);
    run_for(200);
}
static void home_working(void) {
    /* The PC helper reports the user typing right now. */
    uint8_t packet[HELPER_PACKET_LEN] = {HELPER_MAGIC, HELPER_VERSION, HELPER_MSG_IDLE};
    helper_receive(packet, furi_get_tick());
    run_for(500);
}
static void home_bluetooth(void) {
    sim_link = (SimLink){.kind = LinkBluetooth, .connected = false, .on_air = true};
    run_for(500);
}
static void settings_top(void) {
    press(InputKeyRight, InputTypeShort);
}
static void settings_bottom(void) {
    press(InputKeyRight, InputTypeShort);
    press(InputKeyUp, InputTypeShort);
}
static void remote_menu(void) {
    press(InputKeyLeft, InputTypeShort);
}
static void remote_media(void) {
    press(InputKeyLeft, InputTypeShort);
    press(InputKeyOk, InputTypeShort);
    press(InputKeyUp, InputTypeShort);
}
static void remote_slides(void) {
    press(InputKeyLeft, InputTypeShort);
    press(InputKeyDown, InputTypeShort);
    press(InputKeyOk, InputTypeShort);
    press(InputKeyRight, InputTypeShort);
}
static void remote_mouse(void) {
    press(InputKeyLeft, InputTypeShort);
    press(InputKeyUp, InputTypeShort);
    press(InputKeyOk, InputTypeShort);
    press(InputKeyOk, InputTypeShort);
}
static void preview(void) {
    press(InputKeyUp, InputTypeShort);
    run_for(900);
}
static void pairing_waiting(void) {
    sim_link = (SimLink){.kind = LinkBluetooth, .connected = false, .on_air = true};
    pairing_open(&app);
    run_for(500);
}
static void pairing_not_on_air(void) {
    sim_link = (SimLink){.kind = LinkBluetooth, .connected = false, .on_air = false};
    pairing_open(&app);
}
static void pairing_not_on_air_help(void) {
    pairing_not_on_air();
    press(InputKeyRight, InputTypeShort);
}
static void pairing_pc_help(void) {
    pairing_waiting();
    press(InputKeyRight, InputTypeShort);
}
static void pairing_test(void) {
    sim_link = (SimLink){.kind = LinkBluetooth, .connected = true, .on_air = true};
    pairing_open(&app);
}
static void pairing_ask(void) {
    pairing_test();
    press(InputKeyOk, InputTypeShort);
    run_for(1200);
}
static void pairing_failed(void) {
    pairing_ask();
    press(InputKeyLeft, InputTypeShort);
}
static void pairing_all_set(void) {
    pairing_ask();
    press(InputKeyOk, InputTypeShort);
}
static void pairing_usb_busy(void) {
    sim_link = (SimLink){.kind = LinkUsb, .connected = false, .on_air = false};
    pairing_open(&app);
}
static void pairing_usb_unplugged_help(void) {
    sim_link = (SimLink){.kind = LinkUsb, .connected = false, .on_air = true};
    pairing_open(&app);
    press(InputKeyRight, InputTypeShort);
}
static void pairing_usb_all_set(void) {
    sim_link = (SimLink){.kind = LinkUsb, .connected = true, .on_air = true};
    pairing_open(&app);
    press(InputKeyOk, InputTypeShort);
    run_for(1200);
    /* The PC helper is running and the user has been away a minute. */
    uint8_t packet[HELPER_PACKET_LEN] = {HELPER_MAGIC, HELPER_VERSION, HELPER_MSG_IDLE};
    helper_put_u32(packet + 3, 60000);
    helper_receive(packet, furi_get_tick());
    press(InputKeyOk, InputTypeShort);
}
static void pairing_app_help(void) {
    pairing_all_set();
    press(InputKeyRight, InputTypeShort);
}
static void pairing_app_on_help(void) {
    pairing_usb_all_set();
    press(InputKeyRight, InputTypeShort);
}
static void welcome(void) {
    app.screen = ScreenWelcome;
}

static const struct {
    const char* name;
    void (*setup)(void);
} scenes[] = {
    {"home-awake", home_awake},
    {"home-bug", home_bug},
    {"home-bug-quack", home_bug_quack},
    {"home-bug-fixed", home_bug_fixed},
    {"home-sip", home_sip},
    {"home-typing", home_typing},
    {"home-hop", home_hop},
    {"home-asleep", home_asleep},
    {"home-stopped", home_stopped},
    {"home-walking", home_walking},
    {"home-wandering", home_wandering},
    {"home-wander-homing", home_wander_homing},
    {"home-quack", home_quack},
    {"home-working", home_working},
    {"home-bluetooth", home_bluetooth},
    {"settings", settings_top},
    {"settings-bottom", settings_bottom},
    {"remote-menu", remote_menu},
    {"remote-media", remote_media},
    {"remote-slides", remote_slides},
    {"remote-mouse", remote_mouse},
    {"preview", preview},
    {"preview-wandering", preview_wandering},
    {"pairing-waiting", pairing_waiting},
    {"pairing-not-on-air", pairing_not_on_air},
    {"pairing-not-on-air-help", pairing_not_on_air_help},
    {"pairing-pc-help", pairing_pc_help},
    {"pairing-test", pairing_test},
    {"pairing-ask", pairing_ask},
    {"pairing-failed", pairing_failed},
    {"pairing-all-set", pairing_all_set},
    {"pairing-usb-busy", pairing_usb_busy},
    {"pairing-usb-unplugged-help", pairing_usb_unplugged_help},
    {"pairing-usb-all-set", pairing_usb_all_set},
    {"pairing-app-help", pairing_app_help},
    {"pairing-app-on-help", pairing_app_on_help},
    {"welcome", welcome},
};

static bool save_png(const char* folder, const char* name, int scale) {
    int w = SCREEN_W * scale, h = SCREEN_H * scale;
    uint8_t* rgb = malloc((size_t)w * h * 3);
    for(int y = 0; y < h; y++) {
        for(int x = 0; x < w; x++) {
            const uint8_t* c = sim_canvas_pixel(x / scale, y / scale) ? ink : lcd;
            memcpy(&rgb[(y * w + x) * 3], c, 3);
        }
    }
    char path[512];
    snprintf(path, sizeof(path), "%s/%s%s.png", folder, name, scale == 1 ? "" : "@4x");
    bool ok = png_write_rgb(path, w, h, rgb);
    free(rgb);
    return ok;
}

int main(int argc, char** argv) {
    const char* folder = argc > 1 ? argv[1] : ".";
    if(!sim_icons_load()) return 2;

    int problems = 0;
    for(size_t i = 0; i < sizeof(scenes) / sizeof(scenes[0]); i++) {
        fresh();
        scenes[i].setup();
        Canvas* canvas = sim_canvas_get();
        sim_canvas_begin(scenes[i].name);
        draw(canvas);
        if(!save_png(folder, scenes[i].name, 1) || !save_png(folder, scenes[i].name, SCALE)) {
            fprintf(stderr, "%s: can't write the PNG into %s\n", scenes[i].name, folder);
            return 2;
        }
        const char* overflow = sim_canvas_overflow();
        if(overflow[0]) {
            fprintf(stderr, "%s: text runs off the screen\n%s", scenes[i].name, overflow);
            problems++;
        }
        printf("%-26s ok\n", scenes[i].name);
    }
    printf("%d scenes, %d with text off the screen\n", (int)(sizeof(scenes) / sizeof(scenes[0])), problems);
    return problems || unmet ? 1 : 0;
}
