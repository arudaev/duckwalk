#include "helper.h"
#include "helper_protocol.h"
#include "link.h"
#include "util.h"

/* Written from the USB interrupt (helper_receive via the data channel),
 * read from the main and GUI threads. Each is one aligned 32-bit word. */
static volatile uint32_t heard_at;
static volatile uint32_t user_at;
static volatile bool heard;

bool helper_receive(const uint8_t* data, uint32_t now) {
    if(data[0] != HELPER_MAGIC || data[1] != HELPER_VERSION) return false;
    if(data[2] != HELPER_MSG_IDLE) return false;
    uint32_t idle_ms = helper_get_u32(&data[3]);
    /* An idle time longer than the clock has run means "long ago". */
    uint32_t idle = furi_ms_to_ticks(idle_ms);
    user_at = idle < now ? now - idle : 0;
    heard_at = now;
    heard = true;
    return true;
}

static void on_data(const uint8_t* data, void* ctx) {
    UNUSED(ctx);
    helper_receive(data, furi_get_tick());
}

void helper_start(void) {
    link_set_data_callback(on_data, NULL);
}

void helper_stop(void) {
    link_set_data_callback(NULL, NULL);
    heard = false;
}

bool helper_present(void) {
    return heard && tick_before(heard_at + furi_ms_to_ticks(HELPER_TIMEOUT_MS));
}

bool helper_user_active(uint32_t hold_ms) {
    return helper_present() && tick_before(user_at + furi_ms_to_ticks(hold_ms));
}
