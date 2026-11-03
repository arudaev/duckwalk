/* Settings file: a missing or damaged file never stops the app, and every
 * value survives a save and load. */

#include "check.h"
#include "keep_awake.h"
#include "link.h"
#include "settings.h"

#include <furi_hal_rtc.h>
#include <storage/storage.h>

#ifdef _WIN32
#include <direct.h>
#define make_dir(path) _mkdir(path)
#else
#include <sys/stat.h>
#define make_dir(path) mkdir(path, 0755)
#endif

static void write_file(const char* text) {
    FILE* f = fopen(APP_DATA_PATH("settings.txt"), "w");
    fputs(text, f);
    fclose(f);
}

static bool is_default(const DuckwalkSettings* s) {
    DuckwalkSettings d;
    settings_defaults(&d);
    return memcmp(s, &d, sizeof(d)) == 0;
}

int main(void) {
    DuckwalkSettings s, loaded;
    snprintf(shim_app_data_dir, sizeof(shim_app_data_dir), "settings_test_data");
    make_dir(shim_app_data_dir);
    remove(APP_DATA_PATH("settings.txt"));

    /* First launch: no file, defaults, and the caller learns it. */
    CHECK(!settings_load(&s), "missing file reported as found");
    CHECK(is_default(&s), "missing file: not the defaults");

    /* Every value of every setting survives a round trip. */
    for(Setting setting = 0; setting < SettingCount; setting++) {
        for(uint8_t v = 0; v < settings_value_count(setting); v++) {
            settings_defaults(&s);
            s.values[setting] = v;
            settings_save(&s);
            CHECK(settings_load(&loaded), "saved file not found");
            CHECK(loaded.values[setting] == v, "%s = %s came back as %u",
                  settings_label(setting), settings_value_name(setting, v), loaded.values[setting]);
        }
    }
    settings_defaults(&s);
    s.awake_mode = KeepAwakeSubtle;
    settings_save(&s);
    settings_load(&loaded);
    CHECK(loaded.awake_mode == KeepAwakeSubtle, "awake mode came back as %u", loaded.awake_mode);

    /* Damaged files fall back to defaults, line by line. */
    write_file("Filetype: Something else\nVersion: 1\nMode: Subtle\n");
    CHECK(settings_load(&s) && is_default(&s), "wrong filetype was read");

    write_file("Filetype: Duckwalk settings\nVersion: 99\nMode: Subtle\n");
    CHECK(settings_load(&s) && is_default(&s), "wrong version was read");

    write_file("garbage\n\x01\x02\n");
    CHECK(settings_load(&s) && is_default(&s), "garbage was read");

    write_file("Filetype: Duckwalk settings\nVersion: 1\nMode: Sprint\nWalk length: Long\n");
    settings_load(&s);
    CHECK(s.values[SettingMode] == KeepAwakeWalk, "unknown mode not replaced by the default");
    CHECK(s.values[SettingLength] == WalkLengthLong, "valid line after a bad one was lost");

    write_file("Filetype: Duckwalk settings\nVersion: 1\nConnection: Bluetooth");
    settings_load(&s);
    CHECK(s.values[SettingConnection] == LinkBluetooth, "truncated file: last line lost");
    CHECK(s.values[SettingInterval] == IntervalNormal, "truncated file: missing line not default");

    write_file("Filetype: Duckwalk settings\nVersion: 1\nAwake mode: Off\n");
    settings_load(&s);
    CHECK(s.awake_mode == KeepAwakeWalk, "Awake mode: Off must be ignored");

    /* Intervals are ordered and sensible. */
    uint32_t lo, hi, last_hi = 0;
    for(Interval i = 0; i < IntervalCount; i++) {
        settings_interval(i, &lo, &hi);
        CHECK(lo < hi && hi > last_hi && hi <= 4 * 60, "interval %d is %lu-%lu s", i, (unsigned long)lo, (unsigned long)hi);
        last_hi = hi;
    }

    /* Work hours: Monday to Friday, 8:00 to 18:00. */
    settings_defaults(&s);
    s.values[SettingHours] = HoursWork;
    shim_rtc_set(1, 9);
    CHECK(settings_on_duty(&s), "Monday 9:00 off duty");
    shim_rtc_set(5, 8);
    CHECK(settings_on_duty(&s), "Friday 8:00 off duty");
    shim_rtc_set(5, 18);
    CHECK(!settings_on_duty(&s), "Friday 18:00 on duty");
    shim_rtc_set(6, 12);
    CHECK(!settings_on_duty(&s), "Saturday on duty");
    s.values[SettingHours] = HoursAlways;
    CHECK(settings_on_duty(&s), "Always is off duty on Saturday");

    /* Stop after: the time, and Never. */
    settings_defaults(&s);
    CHECK(s.values[SettingStopAfter] == StopAfter4h, "default isn't 4 h");
    CHECK(!settings_stop_due(&s, 4u * 3600000u - 1), "stopped before 4 h");
    CHECK(settings_stop_due(&s, 4u * 3600000u), "didn't stop at 4 h");
    s.values[SettingStopAfter] = StopAfter1h;
    CHECK(settings_stop_due(&s, 3600000u) && !settings_stop_due(&s, 3599999u), "1 h boundary");
    s.values[SettingStopAfter] = StopAfter8h;
    CHECK(!settings_stop_due(&s, 8u * 3600000u - 1) && settings_stop_due(&s, 8u * 3600000u), "8 h boundary");
    s.values[SettingStopAfter] = StopAfterNever;
    CHECK(!settings_stop_due(&s, UINT32_MAX), "Never stopped");

    return check_result("settings");
}
