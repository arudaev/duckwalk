/* The firmware's canvas on a computer: the same u8g2 calls and fonts as
 * applications/services/gui/canvas.c, drawing into a 128x64 buffer the
 * simulator saves as a PNG. Text that runs off the screen is recorded. */

#include "sim.h"

#include <u8g2.h>

struct Canvas {
    u8g2_t fb;
};

static Canvas sim_canvas;
static char overflow[512];
static const char* scene_name = "";

/* The Flipper's 128x64 display as the firmware sets it up (u8g2_glue.c),
 * minus the hardware: a full frame buffer and a display that ignores
 * everything sent to it. */
static const u8x8_display_info_t sim_display_info = {
    .tile_width = 16,
    .tile_height = 8,
    .pixel_width = SCREEN_W,
    .pixel_height = SCREEN_H,
};

static uint8_t sim_display_cb(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    UNUSED(arg_int);
    UNUSED(arg_ptr);
    if(msg == U8X8_MSG_DISPLAY_SETUP_MEMORY) u8x8_d_helper_display_setup_memory(u8x8, &sim_display_info);
    return 1;
}

Canvas* sim_canvas_get(void) {
    static bool ready = false;
    if(!ready) {
        uint8_t tile_rows;
        u8g2_SetupDisplay(&sim_canvas.fb, sim_display_cb, u8x8_cad_001, u8x8_byte_empty, u8x8_dummy_cb);
        uint8_t* buf = u8g2_m_16_8_f(&tile_rows);
        u8g2_SetupBuffer(&sim_canvas.fb, buf, tile_rows, u8g2_ll_hvline_vertical_top_lsb, U8G2_R0);
        ready = true;
    }
    return &sim_canvas;
}

void sim_canvas_begin(const char* scene) {
    scene_name = scene;
    overflow[0] = 0;
    canvas_clear(sim_canvas_get());
    canvas_set_color(&sim_canvas, ColorBlack);
    canvas_set_font(&sim_canvas, FontSecondary);
}

const char* sim_canvas_overflow(void) {
    return overflow;
}

bool sim_canvas_pixel(int x, int y) {
    const uint8_t* buf = u8g2_GetBufferPtr(&sim_canvas.fb);
    return (buf[(y >> 3) * SCREEN_W + x] >> (y & 7)) & 1;
}

static void check_fits(int32_t left, int32_t width, const char* str) {
    if(left >= 0 && left + width <= SCREEN_W) return;
    size_t used = strlen(overflow);
    snprintf(overflow + used, sizeof(overflow) - used, "  \"%s\" spans x %ld..%ld\n", str, (long)left,
             (long)(left + width - 1));
    UNUSED(scene_name);
}

void canvas_clear(Canvas* canvas) {
    u8g2_ClearBuffer(&canvas->fb);
}

void canvas_set_color(Canvas* canvas, Color color) {
    u8g2_SetDrawColor(&canvas->fb, color);
}

void canvas_set_font(Canvas* canvas, Font font) {
    u8g2_SetFontMode(&canvas->fb, 1);
    switch(font) {
    case FontPrimary:
        u8g2_SetFont(&canvas->fb, u8g2_font_helvB08_tr);
        break;
    case FontSecondary:
        u8g2_SetFont(&canvas->fb, u8g2_font_haxrcorp4089_tr);
        break;
    case FontKeyboard:
        u8g2_SetFont(&canvas->fb, u8g2_font_profont11_mr);
        break;
    case FontBigNumbers:
        u8g2_SetFont(&canvas->fb, u8g2_font_profont22_tn);
        break;
    }
}

void canvas_draw_str(Canvas* canvas, int32_t x, int32_t y, const char* str) {
    if(!str) return;
    check_fits(x, u8g2_GetUTF8Width(&canvas->fb, str), str);
    u8g2_DrawUTF8(&canvas->fb, x, y, str);
}

void canvas_draw_str_aligned(Canvas* canvas, int32_t x, int32_t y, Align horizontal, Align vertical, const char* str) {
    if(!str) return;
    int32_t width = u8g2_GetUTF8Width(&canvas->fb, str);
    if(horizontal == AlignRight) x -= width;
    if(horizontal == AlignCenter) x -= width / 2;
    if(vertical == AlignTop) y += u8g2_GetAscent(&canvas->fb);
    if(vertical == AlignCenter) y += u8g2_GetAscent(&canvas->fb) / 2;
    check_fits(x, width, str);
    u8g2_DrawUTF8(&canvas->fb, x, y, str);
}

uint16_t canvas_string_width(Canvas* canvas, const char* str) {
    return str ? u8g2_GetUTF8Width(&canvas->fb, str) : 0;
}

/* Icons are opaque, as on the Flipper: empty pixels paint the other colour. */
void canvas_draw_icon(Canvas* canvas, int32_t x, int32_t y, const Icon* icon) {
    uint8_t color = u8g2_GetDrawColor(&canvas->fb);
    for(int32_t j = 0; j < icon->height; j++) {
        for(int32_t i = 0; i < icon->width; i++) {
            u8g2_SetDrawColor(&canvas->fb, icon->pixels[j * icon->width + i] ? color : !color);
            u8g2_DrawPixel(&canvas->fb, x + i, y + j);
        }
    }
    u8g2_SetDrawColor(&canvas->fb, color);
}

void canvas_draw_dot(Canvas* canvas, int32_t x, int32_t y) {
    u8g2_DrawPixel(&canvas->fb, x, y);
}

void canvas_draw_box(Canvas* canvas, int32_t x, int32_t y, size_t width, size_t height) {
    u8g2_DrawBox(&canvas->fb, x, y, width, height);
}

void canvas_draw_rbox(Canvas* canvas, int32_t x, int32_t y, size_t width, size_t height, size_t radius) {
    u8g2_DrawRBox(&canvas->fb, x, y, width, height, radius);
}

void canvas_draw_frame(Canvas* canvas, int32_t x, int32_t y, size_t width, size_t height) {
    u8g2_DrawFrame(&canvas->fb, x, y, width, height);
}

void canvas_draw_rframe(Canvas* canvas, int32_t x, int32_t y, size_t width, size_t height, size_t radius) {
    u8g2_DrawRFrame(&canvas->fb, x, y, width, height, radius);
}

void canvas_draw_line(Canvas* canvas, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    u8g2_DrawLine(&canvas->fb, x1, y1, x2, y2);
}

void canvas_draw_circle(Canvas* canvas, int32_t x, int32_t y, size_t radius) {
    u8g2_DrawCircle(&canvas->fb, x, y, radius, U8G2_DRAW_ALL);
}

void canvas_draw_disc(Canvas* canvas, int32_t x, int32_t y, size_t radius) {
    u8g2_DrawDisc(&canvas->fb, x, y, radius, U8G2_DRAW_ALL);
}
