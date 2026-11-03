#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>

#include "keep_awake.h"
#include "settings.h"

typedef enum {
    ScreenHome,
    ScreenSettings,
    ScreenRemote,
    ScreenPreview,
    ScreenPairing,
    ScreenWelcome,
} Screen;

typedef enum {
    RemotePageMedia,
    RemotePageSlides,
    RemotePageMouse,
    RemotePageCount,
    RemotePageMenu = RemotePageCount,
} RemotePage;

typedef struct {
    RemotePage page;
    uint8_t selected; /* menu row */
    uint8_t held; /* arrows held in Mouse, one bit each: up, down, left, right */
    uint32_t held_since;
    uint32_t held_seen; /* last press, long or repeat of a held arrow */
    uint8_t lit; /* the label that flashes after a press */
    uint32_t lit_until;
} RemoteState;

/* The bug hunt at Harold's desk (home.c). */
typedef enum {
    DeskReading,
    DeskSpotted, /* "Bug!" */
    DeskQuack, /* he quacks at it */
    DeskFixed, /* "Fixed." */
} DeskPhase;

/* What Harold does at his desk between bug hunts, one every few seconds. */
typedef enum {
    RoutineNone,
    RoutineType, /* types away; the code scrolls faster */
    RoutineSip, /* turns to his coffee; the mug steams up */
    RoutineLook, /* looks round at you */
    RoutineHop, /* a happy little bounce */
    RoutineNod, /* nods off for a moment and shakes it off */
    RoutineCount,
} Routine;

/* Harold's moods on the home screen; all times are ticks. Changed only by
 * home_tick and home_input, read by home_draw. */
typedef struct {
    uint32_t waddle_until;
    uint32_t quack_until;
    uint32_t blink_at; /* the next blink starts here */
    uint32_t blink_until;
    Routine routine;
    uint32_t routine_start;
    uint32_t routine_until;
    uint32_t routine_at; /* the next routine starts here */
    int16_t last_x;
    bool facing_right;
    /* The laptop */
    uint32_t scroll; /* pixels the code has scrolled */
    uint32_t scroll_at;
    DeskPhase desk;
    uint32_t desk_until;
    uint32_t bug_at;
    uint8_t bug_row;
} HomeAnim;

/* The Bluetooth pairing checklist (pairing.c). */
typedef struct {
    bool help; /* showing the fix for the first open box */
    bool testing; /* the pointer test's circle is playing */
    bool asked; /* "did you see the circle?" */
    bool pointer_ok;
    bool pointer_failed;
    uint32_t done_at;
    uint32_t forgot_until;
} PairingState;

/* Slow work a screen asked for. The main loop does it after releasing the
 * app lock, so drawing never waits for it. */
typedef struct {
    bool link; /* start the connection in settings */
    bool save; /* write the settings file */
    bool forget; /* forget every paired computer */
} DuckwalkPending;

/* Where a wander has been lately, for Preview to draw as a fading trail. */
#define TRAIL_LEN 24

typedef struct {
    WalkPoint points[TRAIL_LEN];
    uint8_t head; /* the next slot to write */
    uint8_t count;
    uint32_t at;
} Trail;

typedef struct {
    FuriMutex* mutex;
    NotificationApp* notifications;
    bool running;
    bool skip_save; /* left from Welcome: nothing was chosen yet */
    uint32_t active_at; /* the user last pressed a button or worked on the PC */
    bool stopped; /* put to sleep by Stop after; cleared when woken */
    DuckwalkPending pending;
    Screen screen;
    KeepAwake keep_awake;
    DuckwalkSettings settings;
    uint8_t settings_selected;
    uint8_t settings_top;
    HomeAnim home;
    RemoteState remote;
    uint32_t preview_started;
    Trail trail;
    PairingState pairing;
} Duckwalk;

/* Each screen draws itself and handles its own buttons; ticks run on every
 * pass of the main loop. All of them run with app->mutex held, so none may
 * block: HID reports only queue (link.h), slow work goes in app->pending.
 * Draw functions only read state. */
void home_draw(Canvas* canvas, Duckwalk* app);
void home_input(Duckwalk* app, const InputEvent* input);
void home_tick(Duckwalk* app);
void home_on_move(Duckwalk* app);

void settings_draw(Canvas* canvas, Duckwalk* app);
void settings_input(Duckwalk* app, const InputEvent* input);

void remote_draw(Canvas* canvas, Duckwalk* app);
void remote_input(Duckwalk* app, const InputEvent* input);

/* Moves the pointer while arrows are held in Mouse; call every loop. */
void remote_tick(Duckwalk* app);

void preview_draw(Canvas* canvas, Duckwalk* app);
void preview_input(Duckwalk* app, const InputEvent* input);

/* Records the wander's trail; call every loop. */
void preview_tick(Duckwalk* app);

void welcome_draw(Canvas* canvas, Duckwalk* app);
void welcome_input(Duckwalk* app, const InputEvent* input);

void pairing_open(Duckwalk* app);
void pairing_draw(Canvas* canvas, Duckwalk* app);
void pairing_input(Duckwalk* app, const InputEvent* input);

/* Plays the pointer test and returns home once it passed; call every loop. */
void pairing_tick(Duckwalk* app);

/* Push the current settings into keep-awake, and ask the main loop to
 * switch the connection if it changed. */
void duckwalk_apply_settings(Duckwalk* app);

/* Stop after: puts Harold to sleep once nobody has been around for the
 * set time. `pressed` is true when a button was just pressed; call every
 * loop. */
void duckwalk_check_stop_after(Duckwalk* app, bool pressed);
