#pragma once

/* Duckwalk's Bluetooth profile: the firmware's HID mouse and media keys,
 * plus a small service of Duckwalk's own: the desktop app writes the same
 * 63-byte packets to it as over USB, and Duckwalk notifies it with its own. */

#include <furi_ble/profile_interface.h>

#include "util.h"

typedef void (*BleDuckDataCallback)(const uint8_t* data, void* ctx);

extern const FuriHalBleProfileTemplate* ble_duck;

/* Called from the Bluetooth thread with each packet. */
void ble_duck_set_data_callback(BleDuckDataCallback callback, void* ctx);

/* Notify the computer with a 63-byte packet (from the main thread). */
void ble_duck_notify(const uint8_t* packet);

/* Before the profile goes: takes the helper's service down. */
void ble_duck_stop(void);
