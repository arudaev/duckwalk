#include "keep_awake.h"
#include "usb/usb_duck.h"

#include <furi_hal_random.h>

static uint32_t random_between(uint32_t lo, uint32_t hi) {
    if(hi <= lo) return lo;
    return lo + furi_hal_random_get() % (hi - lo + 1);
}

static void schedule(KeepAwake* ka) {
    ka->next_tick = furi_get_tick() + furi_ms_to_ticks(random_between(ka->min_s, ka->max_s) * 1000);
}

void keep_awake_init(KeepAwake* ka, uint32_t min_s, uint32_t max_s) {
    ka->mode = KeepAwakeOff;
    ka->min_s = min_s;
    ka->max_s = max_s;
    ka->nudges = 0;
    schedule(ka);
}

void keep_awake_set_mode(KeepAwake* ka, KeepAwakeMode mode) {
    ka->mode = mode;
    if(mode != KeepAwakeOff) schedule(ka);
}

uint32_t keep_awake_seconds_left(const KeepAwake* ka) {
    if(ka->mode == KeepAwakeOff) return 0;
    int32_t left = (int32_t)(ka->next_tick - furi_get_tick());
    return left > 0 ? (uint32_t)left / furi_kernel_get_tick_frequency() : 0;
}

/* One pixel out and back, in a random direction, so the pointer never drifts
 * and never follows the same pattern. */
static bool nudge(void) {
    int8_t dx = 0, dy = 0;
    switch(furi_hal_random_get() % 4) {
    case 0:
        dx = 1;
        break;
    case 1:
        dx = -1;
        break;
    case 2:
        dy = 1;
        break;
    default:
        dy = -1;
        break;
    }
    return usb_duck_mouse_move(dx, dy) && usb_duck_mouse_move(-dx, -dy);
}

bool keep_awake_tick(KeepAwake* ka) {
    if(ka->mode == KeepAwakeOff) return false;
    if((int32_t)(furi_get_tick() - ka->next_tick) < 0) return false;
    bool sent = usb_duck_is_connected() && nudge();
    if(sent) ka->nudges++;
    schedule(ka);
    return sent;
}
