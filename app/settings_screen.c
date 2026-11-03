/* Settings: one row per setting, Up and Down to choose, Left and Right to
 * change. Back saves and returns to Harold. */

#include "duckwalk.h"
#include "link.h"

#define ROW_H    10
#define ROWS_TOP 13
#define ROWS_VIS 5

_Static_assert(SettingCount > ROWS_VIS, "the scroll bar divides by the rows out of view");

void duckwalk_apply_settings(Duckwalk* app) {
    DuckwalkSettings* s = &app->settings;
    app->pending.link = true;
    uint32_t min_s, max_s;
    settings_interval(s->values[SettingInterval], &min_s, &max_s);
    keep_awake_set_interval(&app->keep_awake, min_s, max_s);
    keep_awake_set_walk_length(&app->keep_awake, s->values[SettingLength]);
    if(app->keep_awake.mode != s->values[SettingMode]) {
        keep_awake_set_mode(&app->keep_awake, s->values[SettingMode]);
    }
    if(s->values[SettingMode] != KeepAwakeOff) s->awake_mode = s->values[SettingMode];
}

void settings_draw(Canvas* canvas, Duckwalk* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Settings");

    canvas_set_font(canvas, FontSecondary);
    for(uint8_t row = 0; row < ROWS_VIS; row++) {
        uint8_t i = app->settings_top + row;
        if(i >= SettingCount) break;
        int32_t y = ROWS_TOP + row * ROW_H;
        bool selected = i == app->settings_selected;
        if(selected) {
            canvas_draw_rbox(canvas, 0, y, 123, ROW_H, 2);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 4, y + 8, settings_label(i));
        const char* value = settings_value_name(i, app->settings.values[i]);
        canvas_draw_str_aligned(canvas, 115, y + 8, AlignRight, AlignBottom, value);
        if(selected) {
            uint16_t w = canvas_string_width(canvas, value);
            canvas_draw_str(canvas, 115 - w - 6, y + 8, "<");
            canvas_draw_str(canvas, 117, y + 8, ">");
            canvas_set_color(canvas, ColorBlack);
        }
    }

    /* Scroll bar */
    int32_t track = ROWS_VIS * ROW_H;
    int32_t thumb = track * ROWS_VIS / SettingCount;
    int32_t pos = (track - thumb) * app->settings_top / (SettingCount - ROWS_VIS);
    canvas_draw_line(canvas, 126, ROWS_TOP, 126, ROWS_TOP + track - 1);
    canvas_draw_box(canvas, 125, ROWS_TOP + pos, 3, thumb);
}

void settings_input(Duckwalk* app, const InputEvent* input) {
    if(input->type != InputTypeShort && input->type != InputTypeRepeat) return;
    uint8_t* selected = &app->settings_selected;
    uint8_t* value = &app->settings.values[*selected];
    uint8_t count = settings_value_count(*selected);
    /* Each change of Connection restarts the link; holding a button must not
     * flip it back and forth. */
    bool change = input->type == InputTypeShort || *selected != SettingConnection;

    switch(input->key) {
    case InputKeyUp:
        *selected = (*selected + SettingCount - 1) % SettingCount;
        break;
    case InputKeyDown:
        *selected = (*selected + 1) % SettingCount;
        break;
    case InputKeyLeft:
        if(!change) break;
        *value = (*value + count - 1) % count;
        duckwalk_apply_settings(app);
        break;
    case InputKeyRight:
        if(!change) break;
        *value = (*value + 1) % count;
        duckwalk_apply_settings(app);
        break;
    case InputKeyBack:
        if(input->type == InputTypeShort) {
            app->pending.save = true;
            /* The link switches after this input, so ask the setting. */
            if(app->settings.values[SettingConnection] == LinkBluetooth && !link_is_connected()) {
                pairing_open(app);
            } else {
                app->screen = ScreenHome;
            }
        }
        break;
    default:
        break;
    }

    if(*selected < app->settings_top) app->settings_top = *selected;
    if(*selected >= app->settings_top + ROWS_VIS) app->settings_top = *selected - ROWS_VIS + 1;
}
