#pragma once

#include <furi.h>

/* What the user can change on the Settings screen. Saved as plain text in
 * apps_data/duckwalk/settings.txt; a missing or broken file means defaults. */

typedef enum {
    SettingMode,
    SettingConnection,
    SettingInterval,
    SettingLength,
    SettingHours,
    SettingStopAfter,
    SettingSound,
    SettingVibro,
    SettingBabyLock, /* Back leaves only on a double tap or a 2 s hold */
    SettingCount,
} Setting;

typedef enum {
    IntervalQuick, /* 30 s to 1 min */
    IntervalNormal, /* 1 to 3 min */
    IntervalRelaxed, /* 2 to 4 min */
    IntervalCount,
} Interval;

typedef enum {
    HoursAlways,
    HoursWork, /* Monday to Friday, 8:00 to 18:00 */
    HoursCount,
} Hours;

/* How long Harold keeps the computer awake without hearing from you: a
 * button on the Flipper, or the PC helper seeing you work. Then he goes to
 * sleep, so a computer you walked away from still locks in the end. */
typedef enum {
    StopAfter1h,
    StopAfter2h,
    StopAfter4h,
    StopAfter8h,
    StopAfterNever,
    StopAfterCount,
} StopAfter;

typedef struct {
    uint8_t values[SettingCount];
    uint8_t awake_mode; /* the mode OK wakes Harold into */
} DuckwalkSettings;

void settings_defaults(DuckwalkSettings* settings);
/* Returns false when there was no settings file: the first launch. */
bool settings_load(DuckwalkSettings* settings);
void settings_save(const DuckwalkSettings* settings);

const char* settings_label(Setting setting);
const char* settings_value_name(Setting setting, uint8_t value);
uint8_t settings_value_count(Setting setting);

void settings_interval(Interval interval, uint32_t* min_s, uint32_t* max_s);

/* True once nobody has been around for the Stop after time. */
bool settings_stop_due(const DuckwalkSettings* settings, uint32_t idle_ms);

/* True when Harold should be keeping the computer awake right now. */
bool settings_on_duty(const DuckwalkSettings* settings);
