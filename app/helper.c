#include "helper.h"
#include "helper_protocol.h"
#include "link.h"
#include "util.h"

/* Written from the USB interrupt (helper_receive via the data channel),
 * read from the main and GUI threads. Each is one aligned 32-bit word. */
static volatile uint32_t heard_at;
static volatile uint32_t user_at;
static volatile bool heard;
/* A command waiting for the main loop: 0 when there is none. */
static volatile uint8_t command;
static volatile uint8_t command_arg;
/* The latest area report, written from the USB interrupt or the Bluetooth
 * thread. The writer bumps `area_seq` to odd before it writes and back to
 * even after, so a reader that sees the same even number before and after
 * copying got a whole report. The reader gives up after a few tries rather
 * than wait on a writer it may have interrupted. */
static volatile uint32_t area_seq;
static volatile uint32_t area_at;
static volatile PointerArea area_report;

static void read_rect(const uint8_t* p, PointerRect* r) {
    r->left = helper_get_i16(&p[0]);
    r->top = helper_get_i16(&p[2]);
    r->right = helper_get_i16(&p[4]);
    r->bottom = helper_get_i16(&p[6]);
}

static bool rect_ok(const PointerRect* r) {
    return r->left < r->right && r->top < r->bottom;
}

bool helper_receive(const uint8_t* data, uint32_t now) {
    if(data[0] != HELPER_MAGIC || data[1] != HELPER_VERSION) return false;
    if(data[2] == HELPER_MSG_BYE) {
        heard = false;
        return true;
    }
    if(data[2] == HELPER_MSG_COMMAND) {
        if(data[3] < HELPER_CMD_WAKE || data[3] > HELPER_CMD_MODE) return false;
        if(data[3] == HELPER_CMD_MODE && (data[4] < 1 || data[4] > 3)) return false;
        command_arg = data[4];
        command = data[3];
        return true;
    }
    if(data[2] == HELPER_MSG_AREA) {
        PointerArea a = {.x = helper_get_i16(&data[3]), .y = helper_get_i16(&data[5])};
        read_rect(&data[7], &a.safe);
        read_rect(&data[15], &a.screen);
        if(!rect_ok(&a.safe) || !rect_ok(&a.screen)) return false;
        area_seq++;
        area_report = a;
        area_at = now;
        area_seq++;
        return true;
    }
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
    /* Forget a quiet helper for good: after 24.8 days the tick difference
     * would wrap and make it look present again. */
    if(heard && tick_reached(heard_at + furi_ms_to_ticks(HELPER_TIMEOUT_MS))) heard = false;
    return heard;
}

bool helper_area(PointerArea* area, uint32_t* at) {
    if(!helper_present()) return false;
    for(int tries = 0; tries < 4; tries++) {
        uint32_t seq = area_seq;
        *area = area_report;
        *at = area_at;
        if(!(seq & 1) && seq == area_seq) {
            return seq && tick_before(*at + furi_ms_to_ticks(HELPER_AREA_FRESH_MS));
        }
    }
    return false;
}

bool helper_take_command(uint8_t* cmd, uint8_t* arg) {
    uint8_t c = command;
    if(!c) return false;
    *arg = command_arg;
    command = 0;
    *cmd = c;
    return true;
}

bool helper_user_active(uint32_t hold_ms) {
    return helper_present() && tick_before(user_at + furi_ms_to_ticks(hold_ms));
}
