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
} Screen;

/* Harold's moods on the home screen; all times are ticks. */
typedef struct {
    uint32_t waddle_until;
    uint32_t quack_until;
    uint32_t blink_at;
    uint32_t look_until;
    int16_t last_x;
    bool facing_right;
} HomeAnim;

typedef struct {
    FuriMutex* mutex;
    NotificationApp* notifications;
    bool usb_ok;
    bool running;
    Screen screen;
    KeepAwake keep_awake;
    DuckwalkSettings settings;
    uint8_t settings_selected;
    uint8_t settings_top;
    HomeAnim home;
} Duckwalk;

/* Each screen draws itself and handles its own buttons. Both run with
 * app->mutex held. */
void home_draw(Canvas* canvas, Duckwalk* app);
void home_input(Duckwalk* app, const InputEvent* input);
void home_on_move(Duckwalk* app);

void settings_draw(Canvas* canvas, Duckwalk* app);
void settings_input(Duckwalk* app, const InputEvent* input);

/* Push the current settings into keep-awake. */
void duckwalk_apply_settings(Duckwalk* app);
