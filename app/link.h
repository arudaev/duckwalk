#pragma once

#include <furi.h>

/* The connection to the computer: the USB composite HID (usb/usb_duck.h) or
 * Bluetooth through the firmware's BLE HID profile. Everything else in the
 * app talks to the computer through these calls only. One link at a time.
 *
 * Threads: every link_* call comes from the app's main thread. The report
 * calls (move, click, scroll, consumer key) only queue the report and never
 * block, so screens can call them while holding the app lock; the main loop
 * sends the queue with link_flush() after releasing it. link_start, link_stop
 * and link_bt_forget block for up to a few hundred milliseconds and must be
 * called without the app lock held. The status getters only read and are safe
 * from the GUI thread. */

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

/* True when the link isn't running on the Flipper's side, so nothing can
 * see it: USB is held by another app, or Bluetooth isn't on air a few
 * seconds after starting. */
bool link_off(void);

/* The name the computer lists when pairing, e.g. "Harold Rgudalap". */
const char* link_bt_name(void);

/* Forget every computer paired with Duckwalk and advertise again, so the
 * next one can pair from scratch. */
void link_bt_forget(void);

/* Queue a report. Returns false when there is no computer to send it to or
 * the queue is full. A mouse move that fails to send is added to the next
 * one, so the pointer always ends up where the app meant it to be. */
bool link_mouse_move(int8_t dx, int8_t dy);
bool link_mouse_click(uint8_t button);

/* Positive scrolls up (back a slide), negative down. */
bool link_mouse_scroll(int8_t wheel);

/* Press and release a consumer-page key (media keys, volume). */
bool link_consumer_tap(uint16_t usage);

/* Packets from the computer on the data channel: 63 bytes each, called from
 * the USB interrupt or the Bluetooth thread, so keep the callback short. */
typedef void (*LinkDataCallback)(const uint8_t* data, void* ctx);
void link_set_data_callback(LinkDataCallback callback, void* ctx);

/* Queue a 63-byte packet for the computer on the data channel (the vendor
 * report over USB, a notification over Bluetooth). Returns false when there
 * is no computer or the queue is full. */
bool link_send_data(const uint8_t* packet);

/* Send everything queued. Call from the main loop without the app lock. */
void link_flush(void);

/* True while reports are waiting to be sent. */
bool link_busy(void);
