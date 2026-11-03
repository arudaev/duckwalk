/* Remote: a short menu, then one page per job. Media sends media keys,
 * Slides scrolls (next and back in most slide shows), Mouse moves the
 * pointer with the D-pad. Keep-awake keeps running underneath. */

#include "duckwalk.h"
#include "duckwalk_icons.h"
#include "link.h"
#include "util.h"

#include <hid_usage_consumer.h>

#define FLASH_MS       150
#define HELD_QUIET_MS  700
#define MOUSE_MIN_STEP 2
#define MOUSE_MAX_STEP 12
#define MOUSE_RAMP_MS  1200

static const char* const page_names[RemotePageCount] = {
    [RemotePageMedia] = "Media",
    [RemotePageSlides] = "Slides",
    [RemotePageMouse] = "Mouse",
};

/* Labels around the D-pad: up, down, left, right, OK, then a hint line. */
static const char* const page_labels[RemotePageCount][6] = {
    [RemotePageMedia] = {"Vol +", "Vol -", "|<<", ">>|", "Play", "hold OK: mute"},
    [RemotePageSlides] = {"", "", "Back", "Next", "", "scrolls the slides"},
    [RemotePageMouse] = {"^", "v", "<", ">", "Click", "hold OK: right click"},
};

static uint8_t label_index(InputKey key) {
    switch(key) {
    case InputKeyUp:
        return 0;
    case InputKeyDown:
        return 1;
    case InputKeyLeft:
        return 2;
    case InputKeyRight:
        return 3;
    default:
        return 4;
    }
}

static void draw_label(Canvas* canvas, int32_t x, int32_t y, Align align, const char* text, bool lit) {
    if(!text[0]) return;
    uint16_t w = canvas_string_width(canvas, text);
    int32_t left = align == AlignCenter ? x - w / 2 : align == AlignRight ? x - w : x;
    if(lit) {
        canvas_draw_rbox(canvas, left - 3, y - 9, w + 6, 11, 2);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_str(canvas, left, y, text);
    canvas_set_color(canvas, ColorBlack);
}

static void draw_menu(Canvas* canvas, Duckwalk* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Remote");
    canvas_draw_icon(canvas, 116, 1, &I_harold_mini_10x10);
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < RemotePageCount; i++) {
        int32_t y = 16 + i * 13;
        if(i == app->remote.selected) {
            canvas_draw_rbox(canvas, 0, y, 128, 12, 2);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 6, y + 9, page_names[i]);
        canvas_set_color(canvas, ColorBlack);
    }
}

static void draw_page(Canvas* canvas, Duckwalk* app) {
    RemoteState* r = &app->remote;
    const char* const* labels = page_labels[r->page];
    bool flash = tick_before(r->lit_until);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, page_names[r->page]);
    canvas_draw_icon(canvas, 116, 1, &I_harold_mini_10x10);
    if(!link_is_connected()) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 112, 10, AlignRight, AlignBottom, link_status());
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_rframe(canvas, 46, 27, 36, 14, 3);
    draw_label(canvas, 64, 22, AlignCenter, labels[0], flash && r->lit == 0);
    draw_label(canvas, 64, 51, AlignCenter, labels[1], flash && r->lit == 1);
    draw_label(canvas, 40, 37, AlignRight, labels[2], flash && r->lit == 2);
    draw_label(canvas, 88, 37, AlignLeft, labels[3], flash && r->lit == 3);
    draw_label(canvas, 64, 37, AlignCenter, labels[4], flash && r->lit == 4);
    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, labels[5]);
}

void remote_draw(Canvas* canvas, Duckwalk* app) {
    if(app->remote.page == RemotePageMenu) {
        draw_menu(canvas, app);
    } else {
        draw_page(canvas, app);
    }
}

static void menu_input(Duckwalk* app, const InputEvent* input) {
    RemoteState* r = &app->remote;
    if(input->type != InputTypeShort && input->type != InputTypeRepeat) return;
    if(input->key == InputKeyUp) {
        r->selected = (r->selected + RemotePageCount - 1) % RemotePageCount;
    } else if(input->key == InputKeyDown) {
        r->selected = (r->selected + 1) % RemotePageCount;
    } else if(input->key == InputKeyOk && input->type == InputTypeShort) {
        r->page = r->selected;
    } else if(input->key == InputKeyBack && input->type == InputTypeShort) {
        app->screen = ScreenHome;
    }
}

static bool media_key(InputKey key, InputType type) {
    if(type == InputTypeLong && key == InputKeyOk) return link_consumer_tap(HID_CONSUMER_MUTE);
    bool tap = type == InputTypeShort;
    bool hold = type == InputTypeRepeat && (key == InputKeyUp || key == InputKeyDown);
    if(!tap && !hold) return false;
    switch(key) {
    case InputKeyUp:
        return link_consumer_tap(HID_CONSUMER_VOLUME_INCREMENT);
    case InputKeyDown:
        return link_consumer_tap(HID_CONSUMER_VOLUME_DECREMENT);
    case InputKeyLeft:
        return link_consumer_tap(HID_CONSUMER_SCAN_PREVIOUS_TRACK);
    case InputKeyRight:
        return link_consumer_tap(HID_CONSUMER_SCAN_NEXT_TRACK);
    case InputKeyOk:
        return link_consumer_tap(HID_CONSUMER_PLAY_PAUSE);
    default:
        return false;
    }
}

static bool slides_key(InputKey key, InputType type) {
    if(type != InputTypeShort && type != InputTypeRepeat) return false;
    if(key == InputKeyRight) return link_mouse_scroll(-1);
    if(key == InputKeyLeft) return link_mouse_scroll(1);
    return false;
}

static bool mouse_key(RemoteState* r, InputKey key, InputType type) {
    if(key == InputKeyOk) {
        if(type == InputTypeShort) return link_mouse_click(LINK_BUTTON_LEFT);
        if(type == InputTypeLong) return link_mouse_click(LINK_BUTTON_RIGHT);
        return false;
    }
    /* Arrows move while held; remote_tick does the moving. */
    uint8_t bit = 1 << label_index(key);
    if(type == InputTypePress) {
        if(!r->held) r->held_since = furi_get_tick();
        r->held |= bit;
    }
    if(type == InputTypePress || type == InputTypeLong || type == InputTypeRepeat) {
        r->held_seen = furi_get_tick();
        return type == InputTypePress;
    }
    /* Short arrives with the release, so either one ends the hold. */
    r->held &= ~bit;
    return false;
}

void remote_input(Duckwalk* app, const InputEvent* input) {
    RemoteState* r = &app->remote;
    if(r->page == RemotePageMenu) {
        menu_input(app, input);
        return;
    }
    if(input->key == InputKeyBack) {
        if(input->type == InputTypeShort) {
            r->page = RemotePageMenu;
            r->held = 0;
        }
        return;
    }

    bool sent = false;
    switch(r->page) {
    case RemotePageMedia:
        sent = media_key(input->key, input->type);
        break;
    case RemotePageSlides:
        sent = slides_key(input->key, input->type);
        break;
    case RemotePageMouse:
        sent = mouse_key(r, input->key, input->type);
        break;
    default:
        break;
    }
    if(sent) {
        r->lit = label_index(input->key);
        r->lit_until = furi_get_tick() + furi_ms_to_ticks(FLASH_MS);
        /* You are at the computer: no need to walk for a while. */
        keep_awake_postpone(&app->keep_awake);
    }
}

void remote_tick(Duckwalk* app) {
    RemoteState* r = &app->remote;
    if(app->screen != ScreenRemote || r->page != RemotePageMouse || !r->held) return;
    /* A held key repeats every few hundred milliseconds; if it went quiet,
     * its release was lost (a full input queue) and the pointer must stop. */
    if(tick_reached(r->held_seen + furi_ms_to_ticks(HELD_QUIET_MS))) {
        r->held = 0;
        return;
    }

    uint32_t held = furi_get_tick() - r->held_since;
    if(held > MOUSE_RAMP_MS) held = MOUSE_RAMP_MS;
    int8_t step = MOUSE_MIN_STEP + (MOUSE_MAX_STEP - MOUSE_MIN_STEP) * held / MOUSE_RAMP_MS;
    int8_t dx = 0, dy = 0;
    if(r->held & (1 << 0)) dy -= step;
    if(r->held & (1 << 1)) dy += step;
    if(r->held & (1 << 2)) dx -= step;
    if(r->held & (1 << 3)) dx += step;
    if((dx || dy) && link_mouse_move(dx, dy)) {
        r->lit = __builtin_ctz(r->held);
        r->lit_until = furi_get_tick() + furi_ms_to_ticks(FLASH_MS);
        keep_awake_postpone(&app->keep_awake);
    }
}
