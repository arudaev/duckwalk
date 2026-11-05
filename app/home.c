/* Home: Harold, the rubber debugging duck, at his desk.
 *
 * Something is always moving: the code scrolls on his laptop, the coffee
 * steams and the plant sways. Every few seconds Harold does something: he
 * types, sips his coffee, looks round, bounces or nods off for a moment.
 * Every so often a bug turns up in the code: he spots it ("Bug!"), turns
 * round and quacks at it, and it's fixed. Asleep, the laptop is closed, he
 * breathes slowly and the z's float. During a walk or a wander he leaves the
 * desk and moves in step with the cursor. A long OK makes him quack.
 *
 * With Sound on Lively he is heard too: the bug hunt, his hops and his
 * footsteps on a walk, but only while you can see him.
 *
 * home_tick moves the scene on; home_draw only draws it. */

#include "duckwalk.h"
#include "duckwalk_icons.h"
#include "helper.h"
#include "link.h"
#include "util.h"

#include <furi_hal_random.h>

/* Layout, left to right: plant, mug, Harold, laptop. */
#define HOME_X     56 /* Harold's seat at the desk */
#define HAROLD_Y   28 /* his flat bottom sits on the desk line */
#define DESK_Y     52
#define WALK_LEFT  48 /* how far he strays across the screen during a walk, */
#define WALK_RIGHT 20 /* less to the right, where the laptop is */
#define LAPTOP_X   86
#define LAPTOP_Y   28
#define LAPTOP_W   32
#define LAPTOP_H   22
#define MUG_X      38
#define MUG_Y      44
#define PLANT_X    10

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
#define STRIDE_MS  150
#define SWAY_MS    700
#define BREATH_MS  1500
#define ROUTINE_MIN_MS 2500 /* the pause between two routines */
#define ROUTINE_MAX_MS 5000
#define Z_CYCLE_MS 1800
#define SPOT_MS    1200
#define FOOTSTEP_MS 300
#define FIXED_MS   1400
#define BUG_MIN_MS 10000
#define BUG_MAX_MS 25000
/* Baby lock */
#define LOCK_HOLD_MS 2000 /* hold Back this long to leave */
#define LOCK_TAP_MS  600 /* or tap it again within this */
#define LOCK_HINT_MS 2000

static uint32_t ms(uint32_t milliseconds) {
    return furi_ms_to_ticks(milliseconds);
}

static uint32_t random_ms(uint32_t lo, uint32_t hi) {
    return lo + furi_hal_random_get() % (hi - lo + 1);
}

static void schedule_blink(HomeAnim* anim) {
    anim->blink_at = tick_in_ms(random_ms(2000, 6000));
}

/* How long each routine lasts, and how often it comes up (out of 20). */
static const struct {
    uint16_t ms;
    uint8_t weight;
} routines[RoutineCount] = {
    [RoutineType] = {2400, 7},
    [RoutineSip] = {1800, 4},
    [RoutineLook] = {1600, 4},
    [RoutineHop] = {600, 3},
    [RoutineNod] = {1800, 2},
};

static void schedule_routine(HomeAnim* anim) {
    anim->routine = RoutineNone;
    anim->routine_at = tick_in_ms(random_ms(ROUTINE_MIN_MS, ROUTINE_MAX_MS));
}

static void start_routine(HomeAnim* anim) {
    uint32_t roll = furi_hal_random_get() % 20;
    Routine r = RoutineType;
    for(Routine i = RoutineType; i < RoutineCount; i++) {
        if(roll < routines[i].weight) {
            r = i;
            break;
        }
        roll -= routines[i].weight;
    }
    anim->routine = r;
    anim->routine_start = furi_get_tick();
    anim->routine_until = tick_in_ms(routines[r].ms);
}

/* Milliseconds into the routine playing now. */
static uint32_t routine_ms(const HomeAnim* anim) {
    return (furi_get_tick() - anim->routine_start) * 1000 / furi_kernel_get_tick_frequency();
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
    int32_t reach = keep_awake_reach(ka);
    if(reach < 1) reach = 1;
    int32_t x = keep_awake_position(ka).x;
    return HOME_X + x * (x < 0 ? WALK_LEFT : WALK_RIGHT) / reach;
}

typedef enum {
    PoseIdle,
    PoseBob,
    PoseBlink,
    PoseQuack,
} Pose;

/* Each pose facing right and left, and the masks that blank out whatever
 * he stands in front of. */
static const Icon* const sprites[][2] = {
    [PoseIdle] = {&I_harold_idle_24x25, &I_harold_idle_l_24x25},
    [PoseBob] = {&I_harold_bob_24x25, &I_harold_bob_l_24x25},
    [PoseBlink] = {&I_harold_blink_24x25, &I_harold_blink_l_24x25},
    [PoseQuack] = {&I_harold_quack_24x25, &I_harold_quack_l_24x25},
};
static const Icon* const masks[][2] = {
    [PoseIdle] = {&I_harold_idle_mask_24x25, &I_harold_idle_l_mask_24x25},
    [PoseBob] = {&I_harold_bob_mask_24x25, &I_harold_bob_l_mask_24x25},
    [PoseBlink] = {&I_harold_blink_mask_24x25, &I_harold_blink_l_mask_24x25},
    [PoseQuack] = {&I_harold_quack_mask_24x25, &I_harold_quack_l_mask_24x25},
};

typedef struct {
    Pose pose;
    bool left;
    int32_t lift; /* pixels above his seat */
} Frame;

static Frame routine_frame(const HomeAnim* anim) {
    uint32_t t = routine_ms(anim);
    switch(anim->routine) {
    case RoutineType:
        return (Frame){(t / (STRIDE_MS * 2)) % 2 ? PoseBob : PoseIdle, false, 0};
    case RoutineSip:
        /* Turns to the mug for a long sip, then a contented blink. */
        return (Frame){t > 1300 && t < 1500 ? PoseBlink : PoseIdle, t < 1300, 0};
    case RoutineLook:
        return (Frame){t > 700 && t < 850 ? PoseBlink : PoseIdle, true, 0};
    case RoutineHop:
        return (Frame){PoseBob, false, (t / STRIDE_MS) % 2 ? 0 : 2};
    case RoutineNod:
        /* Eyes shut for a moment, then a start: awake again. */
        if(t < 1200) return (Frame){PoseBlink, false, 0};
        return (Frame){t < 1400 ? PoseBob : PoseIdle, false, t < 1400 ? 1 : 0};
    default:
        return (Frame){PoseIdle, false, 0};
    }
}

static Frame harold_frame(const Duckwalk* app) {
    const HomeAnim* anim = &app->home;
    const KeepAwake* ka = &app->keep_awake;
    bool stride = (furi_get_tick() / ms(STRIDE_MS)) % 2;

    if(tick_before(anim->quack_until)) return (Frame){PoseQuack, false, 0};
    if(keep_awake_is_walking(ka)) {
        /* A rubber duck doesn't step, he bobs along. */
        return (Frame){stride ? PoseBob : PoseIdle, !anim->facing_right, 0};
    }
    if(ka->mode == KeepAwakeOff) {
        /* Asleep: a slow breath in and out. */
        return (Frame){PoseBlink, false, (furi_get_tick() / ms(BREATH_MS)) % 2};
    }
    if(tick_before(anim->waddle_until)) return (Frame){stride ? PoseBob : PoseIdle, false, 0};
    /* The bug hunt: he turns round and quacks at you. */
    if(anim->desk == DeskQuack) return (Frame){PoseQuack, true, 0};
    /* You're at the computer: he turns to watch you work. */
    bool blink = tick_before(anim->blink_until);
    if(ka->user_active) return (Frame){blink ? PoseBlink : PoseIdle, true, 0};
    if(anim->routine != RoutineNone) return routine_frame(anim);
    return (Frame){blink ? PoseBlink : PoseIdle, false, 0};
}

static void draw_harold(Canvas* canvas, int32_t x, Frame frame) {
    int32_t y = HAROLD_Y - frame.lift;
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_icon(canvas, x, y, masks[frame.pose][frame.left]);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_icon(canvas, x, y, sprites[frame.pose][frame.left]);
}

/* Harold's own sounds, only while he is on screen. */
static void lively(Duckwalk* app, Sound sound) {
    if(app->screen == ScreenHome || app->screen == ScreenPreview) duckwalk_sound(app, sound);
}

/* Footsteps while a walk plays, left and right. A wander is quiet: it goes
 * on for as long as the mode is on. */
static void footsteps(Duckwalk* app) {
    HomeAnim* anim = &app->home;
    if(!keep_awake_is_playing(&app->keep_awake) || !tick_reached(anim->step_at)) return;
    anim->step_left = !anim->step_left;
    lively(app, anim->step_left ? SoundStepLeft : SoundStepRight);
    anim->step_at = tick_in_ms(FOOTSTEP_MS);
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
    footsteps(app);

    /* Blink every few seconds. */
    if(tick_reached(anim->blink_at)) {
        anim->blink_until = tick_in_ms(BLINK_MS);
        schedule_blink(anim);
    }

    /* The code scrolls while he reads, quicker while he types. */
    bool reading = at_desk(app) && !ka->user_active && anim->desk == DeskReading;
    if(reading && tick_reached(anim->scroll_at)) {
        anim->scroll++;
        anim->scroll_at = tick_in_ms(anim->routine == RoutineType ? SCROLL_MS / 3 : SCROLL_MS);
    }

    /* A routine every few seconds while he reads; none while he's away,
     * busy with a bug or watching you work. */
    if(!reading) {
        anim->routine = RoutineNone;
        if(!anim->routine_at) schedule_routine(anim);
    } else if(anim->routine != RoutineNone) {
        if(tick_reached(anim->routine_until)) schedule_routine(anim);
    } else if(!anim->routine_at) {
        schedule_routine(anim);
    } else if(tick_reached(anim->routine_at)) {
        start_routine(anim);
        if(anim->routine == RoutineHop) lively(app, SoundHop);
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
        } else if(tick_reached(anim->bug_at) && anim->routine == RoutineNone) {
            anim->desk = DeskSpotted;
            anim->bug_row = 2 + furi_hal_random_get() % 3;
            anim->desk_until = tick_in_ms(SPOT_MS);
            lively(app, SoundBugSpotted);
        }
        break;
    case DeskSpotted:
        if(tick_reached(anim->desk_until)) {
            anim->desk = DeskQuack;
            anim->desk_until = tick_in_ms(QUACK_MS);
            lively(app, SoundBugQuack);
        }
        break;
    case DeskQuack:
        if(tick_reached(anim->desk_until)) {
            anim->desk = DeskFixed;
            anim->desk_until = tick_in_ms(FIXED_MS);
            lively(app, SoundBugFixed);
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
        bool bug = row == anim->bug_row && anim->desk != DeskReading;
        /* Room for the bug after the line, inside the screen. */
        if(bug && indent + len > CODE_W - 6) len = CODE_W - 6 - indent;
        canvas_draw_line(canvas, CODE_X + indent, y, CODE_X + indent + len - 1, y);

        if(bug) {
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
    /* Steam that drifts up and sways; more of it while he sips. */
    bool sip = app->home.routine == RoutineSip;
    uint32_t t = furi_get_tick() / ms(sip ? 120 : 250);
    for(uint32_t i = 0; i < (sip ? 3u : 2u); i++) {
        uint32_t phase = (t + i * 2) % 6;
        int32_t sway = (phase % 4 < 2) ? 0 : 1;
        canvas_draw_dot(canvas, MUG_X + 1 + i * 2 + sway, MUG_Y - 2 - (int32_t)phase);
    }
}

/* A potted plant whose leaves sway, faster while Harold bounces. */
static void draw_plant(Canvas* canvas, const Duckwalk* app) {
    int32_t x = PLANT_X, y = DESK_Y;
    canvas_draw_line(canvas, x - 1, y - 8, x + 9, y - 8);
    canvas_draw_line(canvas, x, y - 7, x + 1, y - 1);
    canvas_draw_line(canvas, x + 8, y - 7, x + 7, y - 1);
    canvas_draw_line(canvas, x + 1, y - 1, x + 7, y - 1);

    bool asleep = app->keep_awake.mode == KeepAwakeOff;
    bool hop = app->home.routine == RoutineHop;
    uint32_t period = asleep ? SWAY_MS * 3 : hop ? SWAY_MS / 4 : SWAY_MS;
    int32_t s = (furi_get_tick() / ms(period)) % 2;
    int32_t cx = x + 4;
    /* Three leaves on short stems; the tips move with the sway. */
    canvas_draw_line(canvas, cx, y - 9, cx + s, y - 18);
    canvas_draw_line(canvas, cx + s, y - 18, cx + s + 1, y - 21);
    canvas_draw_line(canvas, cx, y - 10, cx - 4 + s, y - 15);
    canvas_draw_line(canvas, cx - 4 + s, y - 15, cx - 6 + s, y - 15);
    canvas_draw_line(canvas, cx, y - 12, cx + 4 + s, y - 16);
    canvas_draw_line(canvas, cx + 4 + s, y - 16, cx + 6 + s, y - 17);
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
    if(tick_before(app->home.lock_hint_until)) {
        /* Baby lock caught a single Back. */
        snprintf(line, sizeof(line), "Back x2 or hold");
    } else if(keep_awake_is_walking(ka)) {
        snprintf(
            line,
            sizeof(line),
            "%s%.*s",
            keep_awake_is_homing(ka)    ? "Going back" :
            keep_awake_is_wandering(ka) ? "Wandering" :
                                          "Walking",
            (int)(furi_get_tick() / ms(400) % 4),
            "...");
    } else if(ka->mode == KeepAwakeOff && app->stopped) {
        /* Stop after: nobody was around, so the computer may lock. */
        snprintf(line, sizeof(line), "You were away");
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

    draw_plant(canvas, app);
    draw_mug(canvas, app);
    draw_laptop(canvas, app);
    draw_harold(canvas, x, harold_frame(app));
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

/* Back leaves Duckwalk. With Baby lock on, a stray press only says how:
 * tap Back twice, or hold it for 2 s. */
static void back_input(Duckwalk* app, const InputEvent* input) {
    HomeAnim* anim = &app->home;
    if(!app->settings.values[SettingBabyLock]) {
        if(input->type == InputTypeShort) app->running = false;
        return;
    }
    switch(input->type) {
    case InputTypePress:
        anim->back_down_at = furi_get_tick();
        break;
    case InputTypeLong:
    case InputTypeRepeat:
        if(furi_get_tick() - anim->back_down_at >= ms(LOCK_HOLD_MS)) {
            app->running = false;
        } else {
            anim->lock_hint_until = tick_in_ms(LOCK_HINT_MS);
        }
        break;
    case InputTypeShort:
        if(anim->back_tap_at && tick_before(anim->back_tap_at)) {
            app->running = false;
        } else {
            anim->back_tap_at = tick_in_ms(LOCK_TAP_MS);
            anim->lock_hint_until = tick_in_ms(LOCK_HINT_MS);
        }
        break;
    default:
        break;
    }
}

void home_input(Duckwalk* app, const InputEvent* input) {
    KeepAwake* ka = &app->keep_awake;
    DuckwalkSettings* s = &app->settings;
    if(input->key == InputKeyOk && input->type == InputTypeShort) {
        s->values[SettingMode] = ka->mode == KeepAwakeOff ? s->awake_mode : KeepAwakeOff;
        duckwalk_apply_settings(app);
        schedule_blink(&app->home);
        duckwalk_sound(app, ka->mode == KeepAwakeOff ? SoundSleep : SoundWake);
    } else if(input->key == InputKeyOk && input->type == InputTypeLong) {
        app->home.quack_until = tick_in_ms(QUACK_MS);
        duckwalk_sound(app, SoundQuack);
    } else if(input->key == InputKeyRight && input->type == InputTypeShort) {
        app->screen = ScreenSettings;
    } else if(input->key == InputKeyLeft && input->type == InputTypeShort) {
        app->screen = ScreenRemote;
    } else if(input->key == InputKeyDown && input->type == InputTypeShort) {
        pairing_open(app);
    } else if(input->key == InputKeyUp && input->type == InputTypeShort) {
        app->preview_started = furi_get_tick();
        app->screen = ScreenPreview;
    } else if(input->key == InputKeyBack) {
        back_input(app, input);
    }
}

/* A Subtle nudge just went out: Harold wiggles in his seat. */
void home_on_move(Duckwalk* app) {
    if(!keep_awake_is_walking(&app->keep_awake)) {
        app->home.waddle_until = tick_in_ms(WADDLE_MS);
        lively(app, SoundNudge);
    }
}
