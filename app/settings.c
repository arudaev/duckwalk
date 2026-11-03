#include "settings.h"
#include "keep_awake.h"

#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

#define SETTINGS_PATH    APP_DATA_PATH("settings.txt")
#define SETTINGS_TYPE    "Duckwalk settings"
#define SETTINGS_VERSION 1
#define AWAKE_KEY        "Awake mode"

static const char* const mode_names[] = {"Off", "Subtle", "Walk"};
static const char* const interval_names[] = {"30-60 s", "1-3 min", "2-4 min"};
static const char* const length_names[] = {"Short", "Medium", "Long"};
static const char* const hours_names[] = {"Always", "Mon-Fri 8-18"};
static const char* const switch_names[] = {"Off", "On"};

static const struct {
    const char* label;
    const char* const* names;
    uint8_t count;
    uint8_t fallback;
} settings_table[SettingCount] = {
    [SettingMode] = {"Mode", mode_names, KeepAwakeModeCount, KeepAwakeWalk},
    [SettingInterval] = {"Every", interval_names, IntervalCount, IntervalNormal},
    [SettingLength] = {"Walk length", length_names, WalkLengthCount, WalkLengthMedium},
    [SettingHours] = {"Hours", hours_names, HoursCount, HoursAlways},
    [SettingSound] = {"Sound", switch_names, 2, 1},
    [SettingVibro] = {"Vibration", switch_names, 2, 1},
};

void settings_defaults(DuckwalkSettings* settings) {
    for(size_t i = 0; i < SettingCount; i++) settings->values[i] = settings_table[i].fallback;
    settings->awake_mode = KeepAwakeWalk;
}

const char* settings_label(Setting setting) {
    return settings_table[setting].label;
}

const char* settings_value_name(Setting setting, uint8_t value) {
    return settings_table[setting].names[value];
}

uint8_t settings_value_count(Setting setting) {
    return settings_table[setting].count;
}

static bool read_value(FlipperFormat* file, const char* key, Setting setting, FuriString* text, uint8_t* out) {
    if(!flipper_format_rewind(file) || !flipper_format_read_string(file, key, text)) return false;
    for(uint8_t v = 0; v < settings_table[setting].count; v++) {
        if(furi_string_equal_str(text, settings_table[setting].names[v])) {
            *out = v;
            return true;
        }
    }
    return false;
}

void settings_load(DuckwalkSettings* settings) {
    settings_defaults(settings);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* text = furi_string_alloc();
    uint32_t version = 0;

    if(flipper_format_file_open_existing(file, SETTINGS_PATH) &&
       flipper_format_read_header(file, text, &version) &&
       furi_string_equal_str(text, SETTINGS_TYPE) && version == SETTINGS_VERSION) {
        /* Any line that is missing or unreadable keeps its default. */
        for(size_t i = 0; i < SettingCount; i++) {
            read_value(file, settings_table[i].label, i, text, &settings->values[i]);
        }
        uint8_t awake;
        if(read_value(file, AWAKE_KEY, SettingMode, text, &awake) && awake != KeepAwakeOff) {
            settings->awake_mode = awake;
        }
    }

    furi_string_free(text);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
}

void settings_save(const DuckwalkSettings* settings) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);

    bool ok = flipper_format_file_open_always(file, SETTINGS_PATH) &&
              flipper_format_write_header_cstr(file, SETTINGS_TYPE, SETTINGS_VERSION);
    for(size_t i = 0; ok && i < SettingCount; i++) {
        ok = flipper_format_write_string_cstr(
            file, settings_table[i].label, settings_value_name(i, settings->values[i]));
    }
    if(ok) {
        flipper_format_write_string_cstr(file, AWAKE_KEY, mode_names[settings->awake_mode]);
    }

    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
}

void settings_interval(Interval interval, uint32_t* min_s, uint32_t* max_s) {
    static const uint16_t bounds[IntervalCount][2] = {{30, 60}, {60, 180}, {120, 240}};
    *min_s = bounds[interval][0];
    *max_s = bounds[interval][1];
}

bool settings_on_duty(const DuckwalkSettings* settings) {
    if(settings->values[SettingHours] == HoursAlways) return true;
    DateTime now;
    furi_hal_rtc_get_datetime(&now);
    return now.weekday >= 1 && now.weekday <= 5 && now.hour >= 8 && now.hour < 18;
}
