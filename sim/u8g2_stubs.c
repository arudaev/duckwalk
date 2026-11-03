/* The firmware's copy of u8g2 leaves out kerning and screen capture, which
 * a few u8g2 functions the simulator never calls still refer to. Some
 * linkers (MinGW) keep those references, so they get empty stand-ins here. */

#include <u8g2.h>

uint8_t u8g2_GetKerning(u8g2_t* u8g2, u8g2_kerning_t* kerning, uint16_t e1, uint16_t e2) {
    (void)u8g2, (void)kerning, (void)e1, (void)e2;
    return 0;
}

uint8_t u8g2_GetKerningByTable(u8g2_t* u8g2, const uint16_t* kt, uint16_t e1, uint16_t e2) {
    (void)u8g2, (void)kt, (void)e1, (void)e2;
    return 0;
}

uint8_t u8x8_capture_get_pixel_1(uint16_t x, uint16_t y, uint8_t* dest_ptr, uint8_t tile_width) {
    (void)x, (void)y, (void)dest_ptr, (void)tile_width;
    return 0;
}

uint8_t u8x8_capture_get_pixel_2(uint16_t x, uint16_t y, uint8_t* dest_ptr, uint8_t tile_width) {
    (void)x, (void)y, (void)dest_ptr, (void)tile_width;
    return 0;
}

void u8x8_capture_write_pbm_pre(uint8_t tile_width, uint8_t tile_height, void (*out)(const char* s)) {
    (void)tile_width, (void)tile_height, (void)out;
}

void u8x8_capture_write_pbm_buffer(
    uint8_t* buffer,
    uint8_t tile_width,
    uint8_t tile_height,
    uint8_t (*get_pixel)(uint16_t x, uint16_t y, uint8_t* dest_ptr, uint8_t tile_width),
    void (*out)(const char* s)) {
    (void)buffer, (void)tile_width, (void)tile_height, (void)get_pixel, (void)out;
}

void u8x8_capture_write_xbm_pre(uint8_t tile_width, uint8_t tile_height, void (*out)(const char* s)) {
    (void)tile_width, (void)tile_height, (void)out;
}

void u8x8_capture_write_xbm_buffer(
    uint8_t* buffer,
    uint8_t tile_width,
    uint8_t tile_height,
    uint8_t (*get_pixel)(uint16_t x, uint16_t y, uint8_t* dest_ptr, uint8_t tile_width),
    void (*out)(const char* s)) {
    (void)buffer, (void)tile_width, (void)tile_height, (void)get_pixel, (void)out;
}
