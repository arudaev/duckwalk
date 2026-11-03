/* Home: Harold on the desk. Asleep with floating z's when keep-awake is off,
 * blinking and looking around while he waits, walking across the screen in
 * step with the cursor during a walk, and quacking on a long OK. */

#include "duckwalk.h"
#include "duckwalk_icons.h"
#include "usb/usb_duck.h"

#include <furi_hal_random.h>
#include <notification/notification_messages.h>

#define HOME_X      50 /* Harold's spot on the desk */
#define HAROLD_Y    26
#define GROUND_Y    54
#define WALK_SPAN   36 /* how far he strays across the screen during a walk */
#define WADDLE_MS   900
#define QUACK_MS    700
#define BLINK_MS    140
#define LOOK_MS     1600
#define STRIDE_MS   150
#define Z_CYCLE_MS  1800

/* A two-note quack, with or without the buzz, as Settings say. */
static const NotificationSequence sequence_quack = {
    &message_vibro_on,
    &message_note_f4,
    &message_delay_50,
    &message_vibro_off,
    &message_note_d4,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

static const NotificationSequence sequence_quack_silent = {
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    &message_delay_50,
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    NULL,
};

static const NotificationSequence sequence_quack_still = {
    &message_note_f4,
    &message_delay_50,
    &message_note_d4,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

static bool before(uint32_t until) {
    return (int32_t)(until - furi_get_tick()) > 0;
}

static uint32_t ms(uint32_t milliseconds) {
    return furi_ms_to_ticks(milliseconds);
}

static void schedule_blink(HomeAnim* anim) {
    anim->blink_at = furi_get_tick() + ms(2000 + furi_hal_random_get() % 4000);
}

/* Harold's horizontal place on screen: home, or his share of the walk. */
static int32_t harold_x(Duckwalk* app) {
    KeepAwake* ka = &app->keep_awake;
    if(!keep_awake_is_walking(ka)) return HOME_X;
    int16_t reach = walk_reach(&ka->path);
    if(reach < 1) reach = 1;
    return HOME_X + keep_awake_position(ka).x * WALK_SPAN / reach;
}

static const Icon* harold_frame(Duckwalk* app) {
    HomeAnim* anim = &app->home;
    KeepAwake* ka = &app->keep_awake;
    uint32_t now = furi_get_tick();
    bool stride = (now / ms(STRIDE_MS)) % 2;

    if(before(anim->quack_until)) return &I_harold_quack_28x29;

    if(keep_awake_is_walking(ka)) {
        int16_t x = keep_awake_position(ka).x;
        if(x > anim->last_x) anim->facing_right = true;
        if(x < anim->last_x) anim->facing_right = false;
        anim->last_x = x;
        if(anim->facing_right) return stride ? &I_harold_walk_r_1_28x29 : &I_harold_walk_r_0_28x29;
        return stride ? &I_harold_walk_1_28x29 : &I_harold_walk_0_28x29;
    }
    anim->last_x = 0;

    if(ka->mode == KeepAwakeOff) return &I_harold_asleep_28x29;
    if(before(anim->waddle_until)) return stride ? &I_harold_walk_1_28x29 : &I_harold_walk_0_28x29;

    if(!before(anim->blink_at)) {
        if(before(anim->blink_at + ms(BLINK_MS))) return &I_harold_asleep_28x29;
        /* Now and then he looks the other way after a blink. */
        if(furi_hal_random_get() % 4 == 0) anim->look_until = now + ms(LOOK_MS);
        schedule_blink(anim);
    }
    if(before(anim->look_until)) return &I_harold_walk_r_0_28x29;
    return &I_harold_idle_28x29;
}

static void draw_zzz(Canvas* canvas, int32_t x) {
    uint32_t now = furi_get_tick();
    canvas_set_font(canvas, FontSecondary);
    for(uint32_t i = 0; i < 3; i++) {
        uint32_t phase = (now + i * ms(Z_CYCLE_MS) / 3) % ms(Z_CYCLE_MS);
        int32_t rise = phase * 14 / ms(Z_CYCLE_MS);
        canvas_draw_str(canvas, x + 26 + rise / 2, HAROLD_Y + 6 - rise, rise > 9 ? "Z" : "z");
    }
}

static void draw_quack(Canvas* canvas, int32_t x) {
    int32_t bx = x - 38;
    if(bx < 0) bx = 0;
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, bx, HAROLD_Y - 2, 36, 13);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, bx, HAROLD_Y - 2, 36, 13, 3);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, bx + 18, HAROLD_Y + 8, AlignCenter, AlignBottom, "Quack!");
}

static void draw_status(Canvas* canvas, Duckwalk* app) {
    KeepAwake* ka = &app->keep_awake;
    char line[24];
    if(keep_awake_is_walking(ka)) {
        snprintf(line, sizeof(line), "Walking%.*s", (int)(furi_get_tick() / ms(400) % 4), "...");
    } else if(ka->mode == KeepAwakeOff) {
        snprintf(line, sizeof(line), "Asleep. OK wakes him");
    } else if(ka->paused) {
        snprintf(line, sizeof(line), "Off hours");
    } else {
        uint32_t s = keep_awake_seconds_left(ka);
        snprintf(
            line,
            sizeof(line),
            "%s in %lu:%02lu",
            ka->mode == KeepAwakeWalk ? "Walk" : "Nudge",
            s / 60,
            s % 60);
    }
    canvas_draw_str(canvas, 2, 21, line);
}

void home_draw(Canvas* canvas, Duckwalk* app) {
    KeepAwake* ka = &app->keep_awake;
    int32_t x = harold_x(app);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Duckwalk");
    canvas_set_font(canvas, FontSecondary);
    const char* usb = !app->usb_ok              ? "USB busy" :
                      usb_duck_is_connected() ? "USB" :
                                                "no USB";
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, usb);
    draw_status(canvas, app);

    for(int32_t gx = 0; gx < 128; gx += 3) canvas_draw_dot(canvas, gx, GROUND_Y);
    canvas_draw_icon(canvas, x, HAROLD_Y, harold_frame(app));
    if(ka->mode == KeepAwakeOff && !keep_awake_is_walking(ka) && !before(app->home.quack_until)) {
        draw_zzz(canvas, x);
    }
    if(before(app->home.quack_until)) draw_quack(canvas, x);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 63, "< Remote");
    canvas_draw_str_aligned(canvas, 126, 63, AlignRight, AlignBottom, "Settings >");
}

void home_input(Duckwalk* app, const InputEvent* input) {
    KeepAwake* ka = &app->keep_awake;
    DuckwalkSettings* s = &app->settings;
    if(input->key == InputKeyOk && input->type == InputTypeShort) {
        s->values[SettingMode] = ka->mode == KeepAwakeOff ? s->awake_mode : KeepAwakeOff;
        duckwalk_apply_settings(app);
        schedule_blink(&app->home);
    } else if(input->key == InputKeyOk && input->type == InputTypeLong) {
        app->home.quack_until = furi_get_tick() + ms(QUACK_MS);
        bool sound = s->values[SettingSound], vibro = s->values[SettingVibro];
        const NotificationSequence* quack = NULL;
        if(sound && vibro) {
            quack = &sequence_quack;
        } else if(sound) {
            quack = &sequence_quack_still;
        } else if(vibro) {
            quack = &sequence_quack_silent;
        }
        if(quack) notification_message(app->notifications, quack);
    } else if(input->key == InputKeyRight && input->type == InputTypeShort) {
        app->screen = ScreenSettings;
    } else if(input->key == InputKeyLeft && input->type == InputTypeShort) {
        app->screen = ScreenRemote;
    } else if(input->key == InputKeyBack && input->type == InputTypeShort) {
        app->running = false;
    }
}

/* A Subtle nudge just went out: Harold waddles in place. */
void home_on_move(Duckwalk* app) {
    if(!keep_awake_is_walking(&app->keep_awake)) {
        app->home.waddle_until = furi_get_tick() + ms(WADDLE_MS);
    }
}
