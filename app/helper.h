#pragma once

#include <furi.h>

#include "area.h"

/* The PC helper, as the Flipper sees it: is it there, and when did the user
 * last touch their own mouse or keyboard. Fed by packets from the USB data
 * channel (helper_protocol.h); without the helper everything reports "not
 * present" and Duckwalk behaves as before. */

/* Start or stop listening to the data channel. */
void helper_start(void);
void helper_stop(void);

/* Heard from the helper in the last few seconds. */
bool helper_present(void);

/* The helper saw the user's own input less than `hold_ms` ago. */
bool helper_user_active(uint32_t hold_ms);

/* A command from the desktop app (HELPER_CMD_*), once: true and the command
 * and its argument when one is waiting. The newest wins. Main thread. */
bool helper_take_command(uint8_t* cmd, uint8_t* arg);

/* The pointer and the area Harold may take it to, from a report the
 * helper sent in the last HELPER_AREA_FRESH_MS: true and the report and
 * when it came (ticks), or false when there is none. */
bool helper_area(PointerArea* area, uint32_t* at);

/* Parse one packet as received at `now` (ticks). Returns false if it isn't
 * one of ours. Exposed for the host tests; the data channel calls it. */
bool helper_receive(const uint8_t* data, uint32_t now);
