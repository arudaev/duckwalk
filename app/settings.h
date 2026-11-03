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
    SettingSound,
    SettingVibro,
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

typedef struct {
    uint8_t values[SettingCount];
    uint8_t awake_mode; /* the mode OK wakes Harold into */
} DuckwalkSettings;

void settings_defaults(DuckwalkSettings* settings);
void settings_load(DuckwalkSettings* settings);
void settings_save(const DuckwalkSettings* settings);

const char* settings_label(Setting setting);
const char* settings_value_name(Setting setting, uint8_t value);
uint8_t settings_value_count(Setting setting);

void settings_interval(Interval interval, uint32_t* min_s, uint32_t* max_s);

/* True when Harold should be keeping the computer awake right now. */
bool settings_on_duty(const DuckwalkSettings* settings);
