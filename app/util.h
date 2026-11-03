#pragma once

#include <furi.h>

/* Log tag for every FURI_LOG line; read them with `log` in the Flipper's CLI. */
#define TAG "Duckwalk"

/* Tick deadlines that stay correct when the 32-bit tick counter wraps. */

static inline bool tick_reached(uint32_t deadline) {
    return (int32_t)(furi_get_tick() - deadline) >= 0;
}

static inline bool tick_before(uint32_t deadline) {
    return !tick_reached(deadline);
}

static inline uint32_t tick_in_ms(uint32_t milliseconds) {
    return furi_get_tick() + furi_ms_to_ticks(milliseconds);
}
