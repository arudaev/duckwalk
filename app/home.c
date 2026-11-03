/* Home: Harold, the rubber debugging duck, at his desk.
 *
 * While keep-awake waits, he reads code scrolling on his laptop, types a
 * little, blinks and looks around. Every so often a bug turns up in the
 * code: he spots it ("Bug!"), turns round and quacks at it, and it's fixed.
 * Asleep, the laptop is closed and the z's float. During a walk he leaves
 * the desk and walks in step with the cursor. A long OK makes him quack.
 *
 * home_tick moves the scene on; home_draw only draws it. */

#include "duckwalk.h"
#include "duckwalk_icons.h"
#include "helper.h"
#include "link.h"
#include "util.h"

#include <furi_hal_random.h>
#include <notification/notification_messages.h>

/* Layout */
#define HOME_X    50 /* Harold's seat at the desk */
#define HAROLD_Y  28 /* his flat bottom sits on the desk line */
#define DESK_Y    52
#define WALK_SPAN 36 /* how far he strays across the screen during a walk */
#define LAPTOP_X  80
#define LAPTOP_Y  28
#define LAPTOP_W  36
#define LAPTOP_H  22
#define MUG_X     4
#define MUG_Y     44

/* The code on the laptop: rows 3 px apart, scrolling up one pixel at a time. */
#define CODE_X      (LAPTOP_X + 3)
#define CODE_TOP    (LAPTOP_Y + 2)
#define CODE_BOTTOM (LAPTOP_Y + LAPTOP_H - 3)
#define CODE_W      (LAPTOP_W - 6)
#define CODE_ROWS   7
#define SCROLL_MS   500

/* Timing */
#define WADDLE_MS  900
#define QUACK_MS   700
#define BLINK_MS   140
#define LOOK_MS    1600
#define TYPE_MS    1500
#define STRIDE_MS  150
#define Z_CYCLE_MS 1800
#define SPOT_MS    1200
#define FIXED_MS   1400
#define BUG_MIN_MS 10000
#define BUG_MAX_MS 25000

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

static uint32_t ms(uint32_t milliseconds) {
    return furi_ms_to_ticks(milliseconds);
}

static uint32_t random_ms(uint32_t lo, uint32_t hi) {
    return lo + furi_hal_random_get() % (hi - lo + 1);
}

static void schedule_blink(HomeAnim* anim) {
    anim->blink_at = tick_in_ms(random_ms(2000, 6000));
}

static void schedule_bug(HomeAnim* anim) {
    anim->desk = DeskReading;
    anim->bug_at = tick_in_ms(random_ms(BUG_MIN_MS, BUG_MAX_MS));
}

/* Harold sits at the desk unless he is walking or asleep. */
static bool at_desk(const Duckwalk* app) {
    return !keep_awake_is_walking(&app->keep_awake) && app->keep_awake.mode != KeepAwakeOff;
}

/* Harold's horizontal place on screen: his seat, or his share of the walk. */
static int32_t harold_x(const Duckwalk* app) {
    const KeepAwake* ka = &app->keep_awake;
    if(!keep_awake_is_walking(ka)) return HOME_X;
    int16_t reach = keep_awake_reach(ka);
    if(reach < 1) reach = 1;
    return HOME_X + keep_awake_position(ka).x * WALK_SPAN / reach;
}

static const Icon* harold_frame(const Duckwalk* app) {
    const HomeAnim* anim = &app->home;
    const KeepAwake* ka = &app->keep_awake;
    bool stride = (furi_get_tick() / ms(STRIDE_MS)) % 2;

    if(tick_before(anim->quack_until)) return &I_harold_quack_24x25;
    if(keep_awake_is_walking(ka)) {
        /* A rubber duck doesn't step, he bobs along. */
        if(anim->facing_right) return stride ? &I_harold_bob_24x25 : &I_harold_idle_24x25;
        return stride ? &I_harold_bob_l_24x25 : &I_harold_idle_l_24x25;
    }
    if(ka->mode == KeepAwakeOff) return &I_harold_blink_24x25;
    if(tick_before(anim->waddle_until)) return stride ? &I_harold_bob_24x25 : &I_harold_idle_24x25;
    /* The bug hunt: he turns round and quacks at you. */
    if(anim->desk == DeskQuack) return &I_harold_quack_l_24x25;
    /* You're at the computer: he turns to watch you work. */
    if(ka->user_active) return tick_before(anim->blink_until) ? &I_harold_blink_l_24x25 : &I_harold_idle_l_24x25;
    if(tick_before(anim->blink_until)) return &I_harold_blink_24x25;
    if(tick_before(anim->look_until)) return &I_harold_idle_l_24x25;
    if(tick_before(anim->typing_until)) {
        return (furi_get_tick() / ms(STRIDE_MS * 2)) % 2 ? &I_harold_bob_24x25 : &I_harold_idle_24x25;
    }
    return &I_harold_idle_24x25;
}

void home_tick(Duckwalk* app) {
    HomeAnim* anim = &app->home;
    KeepAwake* ka = &app->keep_awake;

    /* Face the way the cursor is going during a walk. */
    if(keep_awake_is_walking(ka)) {
        int16_t x = keep_awake_position(ka).x;
        if(x > anim->last_x) anim->facing_right = true;
        if(x < anim->last_x) anim->facing_right = false;
        anim->last_x = x;
    } else {
        anim->last_x = 0;
    }

    /* Blink every few seconds; now and then look away after, or type. */
    if(tick_reached(anim->blink_at)) {
        anim->blink_until = tick_in_ms(BLINK_MS);
        uint32_t roll = furi_hal_random_get() % 6;
        if(roll == 0) anim->look_until = tick_in_ms(BLINK_MS + LOOK_MS);
        if(roll == 1 || roll == 2) anim->typing_until = tick_in_ms(BLINK_MS + TYPE_MS);
        schedule_blink(anim);
    }

    /* The code scrolls while he reads. */
    bool reading = at_desk(app) && !ka->user_active && anim->desk == DeskReading;
    if(reading && tick_reached(anim->scroll_at)) {
        anim->scroll++;
        anim->scroll_at = tick_in_ms(SCROLL_MS);
    }

    /* The bug hunt: spot it, quack at it, fix it. */
    if(!at_desk(app) || ka->user_active) {
        if(anim->desk != DeskReading || !anim->bug_at) schedule_bug(anim);
        return;
    }
    switch(anim->desk) {
    case DeskReading:
        if(!anim->bug_at) {
            schedule_bug(anim);
        } else if(tick_reached(anim->bug_at)) {
            anim->desk = DeskSpotted;
            anim->bug_row = 2 + furi_hal_random_get() % 3;
            anim->desk_until = tick_in_ms(SPOT_MS);
        }
        break;
    case DeskSpotted:
        if(tick_reached(anim->desk_until)) {
            anim->desk = DeskQuack;
            anim->desk_until = tick_in_ms(QUACK_MS);
        }
        break;
    case DeskQuack:
        if(tick_reached(anim->desk_until)) {
            anim->desk = DeskFixed;
            anim->desk_until = tick_in_ms(FIXED_MS);
        }
        break;
    case DeskFixed:
        if(tick_reached(anim->desk_until)) schedule_bug(anim);
        break;
    }
}

/* A cheap, fixed shuffle so each line of "code" keeps its shape. */
static uint32_t line_hash(uint32_t n) {
    n ^= n >> 16;
    n *= 0x45D9F3Bu;
    n ^= n >> 16;
    return n;
}

static void draw_laptop(Canvas* canvas, const Duckwalk* app) {
    const HomeAnim* anim = &app->home;
    if(app->keep_awake.mode == KeepAwakeOff) {
        /* Closed for the night. */
        canvas_draw_rbox(canvas, LAPTOP_X - 2, DESK_Y - 5, LAPTOP_W + 4, 3, 1);
        canvas_draw_frame(canvas, LAPTOP_X - 4, DESK_Y - 2, LAPTOP_W + 8, 2);
        return;
    }

    canvas_draw_rframe(canvas, LAPTOP_X, LAPTOP_Y, LAPTOP_W, LAPTOP_H, 2);
    canvas_draw_frame(canvas, LAPTOP_X - 4, DESK_Y - 2, LAPTOP_W + 8, 2);
    canvas_draw_line(canvas, LAPTOP_X + 4, LAPTOP_Y + LAPTOP_H, LAPTOP_X + LAPTOP_W - 5, LAPTOP_Y + LAPTOP_H);

    uint32_t first = anim->scroll / 3;
    int32_t shift = anim->scroll % 3;
    for(uint32_t row = 0; row < CODE_ROWS; row++) {
        int32_t y = CODE_TOP + row * 3 - shift;
        if(y < CODE_TOP || y > CODE_BOTTOM) continue;
        uint32_t h = line_hash(first + row);
        if(h % 7 == 0) continue; /* a blank line now and then */
        int32_t indent = (h >> 3) % 3 * 3;
        int32_t len = 4 + (h >> 6) % (CODE_W - 8);
        if(indent + len > CODE_W) len = CODE_W - indent;
        canvas_draw_line(canvas, CODE_X + indent, y, CODE_X + indent + len - 1, y);

        if(row == anim->bug_row && anim->desk != DeskReading) {
            int32_t bx = CODE_X + indent + len + 2;
            if(anim->desk == DeskFixed) {
                /* A tick where the bug was. */
                canvas_draw_line(canvas, bx, y, bx + 1, y + 1);
                canvas_draw_line(canvas, bx + 1, y + 1, bx + 4, y - 2);
            } else if(furi_get_tick() / ms(200) % 2 || anim->desk == DeskQuack) {
                /* The bug: a little blinking beetle. */
                canvas_draw_box(canvas, bx, y - 1, 3, 3);
                canvas_draw_dot(canvas, bx - 1, y - 2);
                canvas_draw_dot(canvas, bx + 3, y - 2);
            }
        }
    }
}

static void draw_mug(Canvas* canvas, const Duckwalk* app) {
    canvas_draw_rframe(canvas, MUG_X, MUG_Y, 7, DESK_Y - MUG_Y, 1);
    canvas_draw_line(canvas, MUG_X + 7, MUG_Y + 2, MUG_X + 8, MUG_Y + 2);
    canvas_draw_line(canvas, MUG_X + 9, MUG_Y + 3, MUG_X + 9, MUG_Y + 4);
    canvas_draw_line(canvas, MUG_X + 7, MUG_Y + 5, MUG_X + 8, MUG_Y + 5);
    if(app->keep_awake.mode == KeepAwakeOff) return;
    /* Steam that drifts up and sways. */
    uint32_t t = furi_get_tick() / ms(250);
    for(uint32_t i = 0; i < 2; i++) {
        uint32_t phase = (t + i * 3) % 6;
        int32_t sway = (phase % 4 < 2) ? 0 : 1;
        canvas_draw_dot(canvas, MUG_X + 2 + i * 2 + sway, MUG_Y - 2 - (int32_t)phase);
    }
}

static void draw_zzz(Canvas* canvas, int32_t x) {
    uint32_t now = furi_get_tick();
    canvas_set_font(canvas, FontSecondary);
    for(uint32_t i = 0; i < 3; i++) {
        uint32_t phase = (now + i * ms(Z_CYCLE_MS) / 3) % ms(Z_CYCLE_MS);
        /* From just in front of his bill up and away over the laptop. */
        int32_t rise = phase * 9 / ms(Z_CYCLE_MS);
        canvas_draw_str(canvas, x + 25 + rise / 2, HAROLD_Y + 12 - rise, rise > 5 ? "Z" : "z");
    }
}

/* A speech bubble to Harold's left. */
static void draw_bubble(Canvas* canvas, int32_t x, const char* text) {
    int32_t bx = x - 37;
    if(bx < 0) bx = 0;
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, bx, HAROLD_Y - 4, 36, 13);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, bx, HAROLD_Y - 4, 36, 13, 3);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, bx + 18, HAROLD_Y + 6, AlignCenter, AlignBottom, text);
}

static void draw_status(Canvas* canvas, const Duckwalk* app) {
    const KeepAwake* ka = &app->keep_awake;
    char line[24];
    if(keep_awake_is_walking(ka)) {
        snprintf(
            line,
            sizeof(line),
            "%s%.*s",
            keep_awake_is_wandering(ka) ? "Wandering" : "Walking",
            (int)(furi_get_tick() / ms(400) % 4),
            "...");
    } else if(ka->mode == KeepAwakeOff) {
        snprintf(line, sizeof(line), "Asleep, OK wakes");
    } else if(ka->paused) {
        snprintf(line, sizeof(line), "Off hours");
    } else if(ka->user_active) {
        /* The PC helper saw the user's own mouse or keyboard. */
        snprintf(line, sizeof(line), "You're working");
    } else {
        uint32_t s = keep_awake_seconds_left(ka);
        snprintf(
            line,
            sizeof(line),
            "%s in %lu:%02lu",
            ka->mode == KeepAwakeSubtle ? "Nudge" : ka->mode == KeepAwakeWalk ? "Walk" : "Wander",
            s / 60,
            s % 60);
    }
    canvas_draw_str(canvas, 2, 21, line);
}

void home_draw(Canvas* canvas, Duckwalk* app) {
    const HomeAnim* anim = &app->home;
    const KeepAwake* ka = &app->keep_awake;
    int32_t x = harold_x(app);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Duckwalk");
    canvas_set_font(canvas, FontSecondary);
    /* "+PC" when the PC helper is talking to us. */
    char link_line[20];
    snprintf(link_line, sizeof(link_line), "%s%s", link_status(), helper_present() ? "+PC" : "");
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, link_line);
    draw_status(canvas, app);
    canvas_draw_str_aligned(canvas, 126, 21, AlignRight, AlignBottom, "^ Preview");

    draw_laptop(canvas, app);
    draw_mug(canvas, app);
    canvas_draw_icon(canvas, x, HAROLD_Y, harold_frame(app));
    /* The desk goes on top: the sprite's blank bottom row would hide it. */
    canvas_draw_line(canvas, 0, DESK_Y, 127, DESK_Y);

    if(ka->mode == KeepAwakeOff && !keep_awake_is_walking(ka) && !tick_before(anim->quack_until)) {
        draw_zzz(canvas, x);
    }
    if(tick_before(anim->quack_until)) {
        draw_bubble(canvas, x, "Quack!");
    } else if(at_desk(app) && !ka->user_active) {
        if(anim->desk == DeskSpotted) draw_bubble(canvas, x, "Bug!");
        if(anim->desk == DeskQuack) draw_bubble(canvas, x, "Quack!");
        if(anim->desk == DeskFixed) draw_bubble(canvas, x, "Fixed.");
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 63, "< Remote");
    /* Down checks the connection; it asks to pair while nothing is. */
    bool unpaired = link_kind() == LinkBluetooth && !link_is_connected();
    const char* down = unpaired ? "Pair" : "Link";
    int32_t left = 64 - (canvas_string_width(canvas, down) + 7) / 2;
    /* A drawn arrow: in this font a "v" reads as a "u". */
    for(int32_t i = 0; i < 3; i++) canvas_draw_line(canvas, left + i, 58 + i, left + 4 - i, 58 + i);
    canvas_draw_str(canvas, left + 7, 63, down);
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
        app->home.quack_until = tick_in_ms(QUACK_MS);
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
    } else if(input->key == InputKeyDown && input->type == InputTypeShort) {
        pairing_open(app);
    } else if(input->key == InputKeyUp && input->type == InputTypeShort) {
        app->preview_started = furi_get_tick();
        app->screen = ScreenPreview;
    } else if(input->key == InputKeyBack && input->type == InputTypeShort) {
        app->running = false;
    }
}

/* A Subtle nudge just went out: Harold wiggles in his seat. */
void home_on_move(Duckwalk* app) {
    if(!keep_awake_is_walking(&app->keep_awake)) {
        app->home.waddle_until = tick_in_ms(WADDLE_MS);
    }
}
