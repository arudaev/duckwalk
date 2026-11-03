#pragma once

#include <furi.h>

/* The connection to the computer: the USB composite HID (usb/usb_duck.h) or
 * Bluetooth through the firmware's BLE HID profile. Everything else in the
 * app talks to the computer through these calls only. One link at a time. */

typedef enum {
    LinkUsb,
    LinkBluetooth,
    LinkCount,
} LinkKind;

#define LINK_BUTTON_LEFT  (1 << 0)
#define LINK_BUTTON_RIGHT (1 << 1)

/* Takes over the USB port, or starts advertising over Bluetooth. Starting a
 * different kind stops the current one first. */
void link_start(LinkKind kind);

/* Gives USB back to its previous mode, or restores the default Bluetooth
 * profile. */
void link_stop(void);

LinkKind link_kind(void);
bool link_is_connected(void);

/* A few words for the screen, e.g. "USB", "BT pairing". */
const char* link_status(void);

/* The name the computer lists when pairing, e.g. "Harold Rgudalap". */
const char* link_bt_name(void);

/* Forget every computer paired with Duckwalk and advertise again, so the
 * next one can pair from scratch. */
void link_bt_forget(void);

bool link_mouse_move(int8_t dx, int8_t dy);
bool link_mouse_click(uint8_t button);

/* Positive scrolls up (back a slide), negative down. */
bool link_mouse_scroll(int8_t wheel);

/* Press and release a consumer-page key (media keys, volume). */
bool link_consumer_tap(uint16_t usage);
