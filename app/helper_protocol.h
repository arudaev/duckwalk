#pragma once

/* What the PC helper (desktop/helper) and the Flipper say to each other over
 * the USB data channel: 63-byte vendor reports, report ID 4 on the wire (see
 * usb/usb_duck.h). Shared by both sides; plain C, no Flipper headers.
 *
 * Every packet starts with the magic byte and the protocol version:
 *   [0] 'D'  [1] version  [2] message type  [3..] message data
 *
 * PC -> Flipper
 *   HELPER_MSG_IDLE: [3..6] milliseconds since the user last touched a mouse
 *   or keyboard other than Duckwalk, little-endian. Sent once a second, and
 *   straight away (at most every 250 ms) when the user is active. */

#include <stdint.h>

#define HELPER_MAGIC   'D'
#define HELPER_VERSION 1

#define HELPER_MSG_IDLE 0x01

#define HELPER_PACKET_LEN 63

/* How often the helper reports at least, and how long the Flipper waits
 * before deciding the helper is gone. */
#define HELPER_HEARTBEAT_MS 1000
#define HELPER_TIMEOUT_MS   5000

static inline void helper_put_u32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static inline uint32_t helper_get_u32(const uint8_t* p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
