#pragma once

/* Duckwalk's Bluetooth profile: the firmware's HID mouse and media keys,
 * plus a small service of Duckwalk's own that the PC helper writes the same
 * 63-byte packets to as over USB. */

#include <furi_ble/profile_interface.h>

#include "util.h"

typedef void (*BleDuckDataCallback)(const uint8_t* data, void* ctx);

extern const FuriHalBleProfileTemplate* ble_duck;

/* Called from the Bluetooth thread with each packet. */
void ble_duck_set_data_callback(BleDuckDataCallback callback, void* ctx);

/* Before the profile goes: takes the helper's service down. */
void ble_duck_stop(void);
