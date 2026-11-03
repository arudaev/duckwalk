#include "link.h"
#include "usb/usb_duck.h"

#include <furi_hal.h>
#include <bt/bt_service/bt.h>
#include <storage/storage.h>
#include <extra_profiles/hid_profile.h>

/* Bonds live in the app's own folder, so pairing Duckwalk never touches the
 * Flipper's main Bluetooth pairing. */
#define BT_KEYS_PATH APP_DATA_PATH(".bt_hid.keys")

/* The host sees "Harold <Flipper name>". The XOR keeps Duckwalk's address
 * apart from the firmware's own Bluetooth Remote app. */
static const BleProfileHidParams bt_params = {
    .device_name_prefix = "Harold",
    .mac_xor = 0x0003,
};

static struct {
    bool started;
    LinkKind kind;
    /* USB */
    bool usb_ok;
    FuriHalUsbInterface* usb_prev;
    /* Bluetooth */
    Bt* bt;
    FuriHalBleProfileBase* profile;
    volatile BtStatus bt_status;
} link;

static void bt_status_changed(BtStatus status, void* ctx) {
    UNUSED(ctx);
    link.bt_status = status;
}

static void usb_start(void) {
    link.usb_prev = furi_hal_usb_get_config();
    for(int i = 0; i < 20 && !(link.usb_ok = furi_hal_usb_set_config(&usb_duck, NULL)); i++) {
        furi_delay_ms(100);
    }
}

static void usb_stop(void) {
    if(link.usb_ok) furi_hal_usb_set_config(link.usb_prev, NULL);
    link.usb_ok = false;
}

static void bt_start(void) {
    link.bt = furi_record_open(RECORD_BT);
    link.bt_status = BtStatusOff;
    bt_disconnect(link.bt);
    furi_delay_ms(200); /* let the radio core drop the old connection */
    bt_keys_storage_set_storage_path(link.bt, BT_KEYS_PATH);
    link.profile = bt_profile_start(link.bt, ble_profile_hid, (void*)&bt_params);
    if(link.profile) {
        furi_hal_bt_start_advertising();
        bt_set_status_changed_callback(link.bt, bt_status_changed, NULL);
    }
}

static void bt_stop(void) {
    bt_set_status_changed_callback(link.bt, NULL, NULL);
    bt_disconnect(link.bt);
    furi_delay_ms(200);
    bt_keys_storage_set_default_path(link.bt);
    bt_profile_restore_default(link.bt);
    furi_record_close(RECORD_BT);
    link.bt = NULL;
    link.profile = NULL;
}

void link_start(LinkKind kind) {
    if(link.started && link.kind == kind) return;
    link_stop();
    link.kind = kind;
    link.started = true;
    if(kind == LinkUsb) {
        usb_start();
    } else {
        bt_start();
    }
}

void link_stop(void) {
    if(!link.started) return;
    if(link.kind == LinkUsb) {
        usb_stop();
    } else {
        bt_stop();
    }
    link.started = false;
}

LinkKind link_kind(void) {
    return link.kind;
}

bool link_is_connected(void) {
    if(!link.started) return false;
    if(link.kind == LinkUsb) return link.usb_ok && usb_duck_is_connected();
    return link.profile && link.bt_status == BtStatusConnected;
}

const char* link_status(void) {
    if(link.kind == LinkUsb) {
        if(!link.usb_ok) return "USB busy";
        return usb_duck_is_connected() ? "USB" : "no USB";
    }
    if(!link.profile) return "BT failed";
    switch(link.bt_status) {
    case BtStatusConnected:
        return "BT";
    case BtStatusAdvertising:
        return "BT pairing";
    case BtStatusOff:
        return "BT off";
    default:
        return "no BT";
    }
}

bool link_mouse_move(int8_t dx, int8_t dy) {
    if(!link_is_connected()) return false;
    if(link.kind == LinkUsb) return usb_duck_mouse_move(dx, dy);
    return ble_profile_hid_mouse_move(link.profile, dx, dy);
}

bool link_mouse_click(uint8_t button) {
    if(!link_is_connected()) return false;
    if(link.kind == LinkUsb) return usb_duck_mouse_click(button);
    return ble_profile_hid_mouse_press(link.profile, button) &&
           ble_profile_hid_mouse_release(link.profile, button);
}

bool link_mouse_scroll(int8_t wheel) {
    if(!link_is_connected()) return false;
    if(link.kind == LinkUsb) return usb_duck_mouse_scroll(wheel);
    return ble_profile_hid_mouse_scroll(link.profile, wheel);
}

bool link_consumer_tap(uint16_t usage) {
    if(!link_is_connected()) return false;
    if(link.kind == LinkUsb) return usb_duck_consumer_tap(usage);
    return ble_profile_hid_consumer_key_press(link.profile, usage) &&
           ble_profile_hid_consumer_key_release(link.profile, usage);
}
