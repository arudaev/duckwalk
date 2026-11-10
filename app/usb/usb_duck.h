#pragma once

#include <furi.h>
#include <furi_hal_usb.h>

/* One HID interface, three top-level collections:
 *   report 2 – mouse (buttons, x, y, wheel)
 *   report 3 – consumer control (media keys)
 *   report 4 – vendor data, 63 bytes each way
 * The host OS owns the mouse and consumer collections; the vendor collection
 * gets its own device path that hidapi can open. */

/* Duckwalk's own pid.codes ID, https://pid.codes/1209/D0C4/. Every place
 * that uses it is listed in docs/usb-device.md. */
#define USB_DUCK_VID 0x1209
#define USB_DUCK_PID 0xD0C4

#define USB_DUCK_REPORT_MOUSE    2
#define USB_DUCK_REPORT_CONSUMER 3
#define USB_DUCK_REPORT_VENDOR   4

#define USB_DUCK_VENDOR_LEN 63

typedef struct {
    uint8_t data[USB_DUCK_VENDOR_LEN];
} UsbDuckVendorPacket;

extern FuriHalUsbInterface usb_duck;

/* Called from the USB interrupt for every vendor OUT report; keep it short
 * (post to a queue). */
typedef void (*UsbDuckRxCallback)(const UsbDuckVendorPacket* packet, void* ctx);
void usb_duck_set_rx_callback(UsbDuckRxCallback callback, void* ctx);

/* Free what the interface allocated. Call after switching USB back to
 * another interface, never while usb_duck is active. */
void usb_duck_free(void);

bool usb_duck_is_connected(void);
bool usb_duck_mouse_move(int8_t dx, int8_t dy);

#define USB_DUCK_BUTTON_LEFT  (1 << 0)
#define USB_DUCK_BUTTON_RIGHT (1 << 1)

/* Press and release a mouse button. */
bool usb_duck_mouse_click(uint8_t button);

/* Positive scrolls up (back a slide), negative down. */
bool usb_duck_mouse_scroll(int8_t wheel);
bool usb_duck_consumer_tap(uint16_t usage);
bool usb_duck_vendor_send(const uint8_t* data, size_t len);
