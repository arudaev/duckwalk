#pragma once

/* What the desktop app (desktop/app) and the Flipper say to each other over
 * the USB data channel: 63-byte vendor reports, report ID 4 on the wire (see
 * usb/usb_duck.h). Shared by both sides; plain C, no Flipper headers.
 *
 * Every packet starts with the magic byte and the protocol version:
 *   [0] 'D'  [1] version  [2] message type  [3..] message data
 *
 * PC -> Flipper
 *   HELPER_MSG_IDLE: [3..6] milliseconds since the user last touched a mouse
 *   or keyboard other than Duckwalk, little-endian. Sent once a second, and
 *   straight away (at most every 250 ms) when the user is active.
 *   HELPER_MSG_BYE: no data. The helper is quitting; forget it now instead
 *   of after HELPER_TIMEOUT_MS. Older Flipper builds ignore it.
 *   HELPER_MSG_COMMAND: [3] HELPER_CMD_*, [4] for HELPER_CMD_MODE the mode
 *   (1 subtle, 2 walk, 3 wander). As if pressed on the Flipper: wake and
 *   sleep do what OK does, and a mode is set as in Settings.
 *   HELPER_MSG_AREA: where the pointer is and where Harold may take it, in
 *   the computer's pixels, signed 16-bit little-endian: [3..6] the pointer
 *   (x, y), [7..14] the safe area to wander in (left, top, right, bottom:
 *   the middle of the active window, clear of its edges and the taskbar),
 *   [15..22] the screen the pointer is on. Sent with each idle packet and
 *   every HELPER_AREA_EVERY_MS while Harold has the pointer. Older Flipper
 *   builds ignore it.
 *
 * Flipper -> PC (types 0x81 and up, so the two directions never mix; each
 * side ignores types it doesn't know)
 *   HELPER_MSG_STATE: [3] flags (HELPER_STATE_*), [4] keep-awake mode
 *   (0 off, 1 subtle, 2 walk, 3 wander), [5] connection (0 USB, 1 Bluetooth).
 *   Sent when something changes and every HELPER_STATE_EVERY_MS.
 *   HELPER_MSG_WALK: a walk starting, in pieces. [3..4] total steps, [5..6]
 *   index of the first step here, [7] steps here (up to HELPER_WALK_STEPS),
 *   [8] milliseconds per step, then (dx, dy) per step as signed bytes,
 *   relative to the step before (the first to where the walk starts).
 *   HELPER_MSG_QUACK: no data. Harold quacks.
 *   HELPER_MSG_CLOSING: no data. Duckwalk is closing on the Flipper.
 * Over Bluetooth both directions share one characteristic: the app writes
 * to it, Duckwalk notifies on it. */

#include <stdint.h>

#define HELPER_MAGIC   'D'
#define HELPER_VERSION 1

#define HELPER_MSG_IDLE 0x01
#define HELPER_MSG_BYE  0x02
#define HELPER_MSG_COMMAND 0x03
#define HELPER_MSG_AREA    0x04

#define HELPER_CMD_WAKE  1
#define HELPER_CMD_SLEEP 2
#define HELPER_CMD_MODE  3

#define HELPER_MSG_STATE   0x81
#define HELPER_MSG_WALK    0x82
#define HELPER_MSG_QUACK   0x83
#define HELPER_MSG_CLOSING 0x84

#define HELPER_STATE_AWAKE   (1 << 0) /* keep-awake is on */
#define HELPER_STATE_WALKING (1 << 1) /* Harold has the pointer */
#define HELPER_STATE_WAITING (1 << 2) /* holding back while the user works */

#define HELPER_STATE_EVERY_MS 2000
#define HELPER_WALK_HEADER    9
#define HELPER_WALK_STEPS     27 /* (63 - 9) / 2 */

#define HELPER_PACKET_LEN 63

/* How often the helper reports at least, and how long the Flipper waits
 * before deciding the helper is gone. */
#define HELPER_HEARTBEAT_MS 1000
#define HELPER_TIMEOUT_MS   5000

/* While Harold has the pointer the helper reports the area this often, and
 * the Flipper trusts a report this long. */
#define HELPER_AREA_EVERY_MS 100
#define HELPER_AREA_FRESH_MS 2000

static inline void helper_put_u32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static inline uint32_t helper_get_u32(const uint8_t* p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static inline void helper_put_u16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static inline int16_t helper_get_i16(const uint8_t* p) {
    return (int16_t)(uint16_t)(p[0] | p[1] << 8);
}

static inline uint16_t helper_get_u16(const uint8_t* p) {
    return (uint16_t)(p[0] | p[1] << 8);
}
