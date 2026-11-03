#pragma once

#include <stdbool.h>
#include <stdint.h>

/* A grey value per pixel (0 black .. 255 white), or NULL if the file isn't a
 * PNG this reader understands. Free with free(). */
uint8_t* png_read_gray(const char* path, int* width, int* height);

/* 8-bit RGB, rows top to bottom. */
bool png_write_rgb(const char* path, int width, int height, const uint8_t* rgb);
