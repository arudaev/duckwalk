#pragma once

#include <stdint.h>

/* Where the pointer is on the computer and where Harold may take it, as the
 * desktop app reports it (HELPER_MSG_AREA), in the computer's pixels. The
 * right and bottom edges are just outside, like a Windows RECT. */
typedef struct {
    int16_t left, top, right, bottom;
} PointerRect;

typedef struct {
    int16_t x, y; /* the pointer */
    PointerRect safe; /* the middle of the active window: Wander stays here */
    PointerRect screen; /* the screen the pointer is on: a walk stays here */
} PointerArea;
