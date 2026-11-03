#pragma once

#include <furi.h>

/* Keep-awake scheduling. Subtle mode: at a random moment inside
 * [min_s, max_s] seconds, move the pointer one pixel and back. The OS idle
 * timer resets on any input, so the computer stays awake without the
 * pointer visibly drifting. */

typedef enum {
    KeepAwakeOff,
    KeepAwakeSubtle,
} KeepAwakeMode;

typedef struct {
    KeepAwakeMode mode;
    uint32_t min_s;
    uint32_t max_s;
    uint32_t next_tick;
    uint32_t nudges;
} KeepAwake;

void keep_awake_init(KeepAwake* ka, uint32_t min_s, uint32_t max_s);
void keep_awake_set_mode(KeepAwake* ka, KeepAwakeMode mode);

/* Seconds until the next nudge (0 when off). */
uint32_t keep_awake_seconds_left(const KeepAwake* ka);

/* Call often. Returns true when a nudge was sent this call. */
bool keep_awake_tick(KeepAwake* ka);
