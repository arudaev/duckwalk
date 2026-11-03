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
    uint8_t lit; /* the label that flashes after a press */
    uint32_t lit_until;
} RemoteState;

/* Harold's moods on the home screen; all times are ticks. */
typedef struct {
    uint32_t waddle_until;
    uint32_t quack_until;
    uint32_t blink_at;
    uint32_t look_until;
    int16_t last_x;
    bool facing_right;
} HomeAnim;

/* The Bluetooth pairing checklist (pairing.c). */
typedef struct {
    bool help; /* showing the fix for the first open box */
    uint8_t circle_step; /* pointer test in progress; 0 when not */
    uint32_t circle_tick;
    bool asked; /* "did you see the circle?" */
    bool pointer_ok;
    bool pointer_failed;
    uint32_t done_at;
    uint32_t forgot_until;
} PairingState;

typedef struct {
    FuriMutex* mutex;
    NotificationApp* notifications;
    bool running;
    Screen screen;
    KeepAwake keep_awake;
    DuckwalkSettings settings;
    uint8_t settings_selected;
    uint8_t settings_top;
    HomeAnim home;
    RemoteState remote;
    uint32_t preview_started;
    PairingState pairing;
} Duckwalk;

/* Each screen draws itself and handles its own buttons. Both run with
 * app->mutex held. */
void home_draw(Canvas* canvas, Duckwalk* app);
void home_input(Duckwalk* app, const InputEvent* input);
void home_on_move(Duckwalk* app);

void settings_draw(Canvas* canvas, Duckwalk* app);
void settings_input(Duckwalk* app, const InputEvent* input);

void remote_draw(Canvas* canvas, Duckwalk* app);
void remote_input(Duckwalk* app, const InputEvent* input);

/* Moves the pointer while arrows are held in Mouse; call every loop. */
void remote_tick(Duckwalk* app);

void preview_draw(Canvas* canvas, Duckwalk* app);
void preview_input(Duckwalk* app, const InputEvent* input);

void welcome_draw(Canvas* canvas, Duckwalk* app);
void welcome_input(Duckwalk* app, const InputEvent* input);

void pairing_open(Duckwalk* app);
void pairing_draw(Canvas* canvas, Duckwalk* app);
void pairing_input(Duckwalk* app, const InputEvent* input);

/* Plays the pointer test and returns home once it passed; call every loop. */
void pairing_tick(Duckwalk* app);

/* Push the current settings into keep-awake. */
void duckwalk_apply_settings(Duckwalk* app);
