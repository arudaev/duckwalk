#pragma once

/* The firmware's Canvas API, as far as Duckwalk's screens use it, drawn by
 * sim/canvas.c through u8g2 like the firmware's own canvas.c. */

#include <furi.h>

typedef struct Canvas Canvas;

typedef enum {
    ColorWhite = 0x00,
    ColorBlack = 0x01,
    ColorXOR = 0x02,
} Color;

typedef enum {
    FontPrimary,
    FontSecondary,
    FontKeyboard,
    FontBigNumbers,
} Font;

typedef enum {
    AlignLeft,
    AlignRight,
    AlignTop,
    AlignBottom,
    AlignCenter,
} Align;

/* Icons: loaded from app/images at start-up (see sim/icons.c). */
typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t* pixels; /* one byte per pixel, 1 = ink */
    const char* file;
} Icon;

void canvas_clear(Canvas* canvas);
void canvas_set_color(Canvas* canvas, Color color);
void canvas_set_font(Canvas* canvas, Font font);
void canvas_draw_str(Canvas* canvas, int32_t x, int32_t y, const char* str);
void canvas_draw_str_aligned(Canvas* canvas, int32_t x, int32_t y, Align horizontal, Align vertical, const char* str);
uint16_t canvas_string_width(Canvas* canvas, const char* str);
void canvas_draw_icon(Canvas* canvas, int32_t x, int32_t y, const Icon* icon);
void canvas_draw_dot(Canvas* canvas, int32_t x, int32_t y);
void canvas_draw_box(Canvas* canvas, int32_t x, int32_t y, size_t width, size_t height);
void canvas_draw_rbox(Canvas* canvas, int32_t x, int32_t y, size_t width, size_t height, size_t radius);
void canvas_draw_frame(Canvas* canvas, int32_t x, int32_t y, size_t width, size_t height);
void canvas_draw_rframe(Canvas* canvas, int32_t x, int32_t y, size_t width, size_t height, size_t radius);
void canvas_draw_line(Canvas* canvas, int32_t x1, int32_t y1, int32_t x2, int32_t y2);
void canvas_draw_circle(Canvas* canvas, int32_t x, int32_t y, size_t radius);
void canvas_draw_disc(Canvas* canvas, int32_t x, int32_t y, size_t radius);
