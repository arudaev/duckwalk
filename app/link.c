#include "link.h"
#include "util.h"
#include "usb/usb_duck.h"

#include <furi_hal.h>
#include <bt/bt_service/bt.h>
#include <storage/storage.h>
#include <extra_profiles/hid_profile.h>

/* Bonds live in the app's own folder, so pairing Duckwalk never touches the
 * Flipper's main Bluetooth pairing. */
#define BT_KEYS_PATH APP_DATA_PATH(".bt_hid.keys")

/* How long Bluetooth may take to report advertising before the pairing
 * screen says it isn't on air. */
#define BT_START_GRACE_MS 3000

#define OUTBOX_SIZE 64

/* The host sees "Harold <Flipper name>". The XOR keeps Duckwalk's address
 * apart from the firmware's own Bluetooth Remote app. */
static const BleProfileHidParams bt_params = {
    .device_name_prefix = "Harold",
    .mac_xor = 0x0003,
};

typedef enum {
    ReportMove,
    ReportClick,
    ReportScroll,
    ReportConsumer,
} ReportType;

typedef struct {
    ReportType type;
    int16_t a;
    int16_t b;
} Report;

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
    uint32_t bt_started_at;
    /* Reports waiting for link_flush, and mouse movement still owed to the
     * computer after a failed send. */
    Report outbox[OUTBOX_SIZE];
    uint8_t head;
    uint8_t count;
    int16_t owed_x;
    int16_t owed_y;
    bool failing;
    /* The data channel */
    LinkDataCallback data_callback;
    void* data_ctx;
} link;

static void bt_status_changed(BtStatus status, void* ctx) {
    UNUSED(ctx);
    link.bt_status = status;
}

/* USB interrupt context. */
static void usb_data(const UsbDuckVendorPacket* packet, void* ctx) {
    UNUSED(ctx);
    LinkDataCallback callback = link.data_callback;
    if(callback) callback(packet->data, link.data_ctx);
}

void link_set_data_callback(LinkDataCallback callback, void* ctx) {
    link.data_callback = NULL;
    link.data_ctx = ctx;
    link.data_callback = callback;
}

static void usb_start(void) {
    usb_duck_set_rx_callback(usb_data, NULL);
    link.usb_prev = furi_hal_usb_get_config();
    for(int i = 0; i < 20 && !(link.usb_ok = furi_hal_usb_set_config(&usb_duck, NULL)); i++) {
        furi_delay_ms(100);
    }
    FURI_LOG_I(TAG, "USB %s", link.usb_ok ? "taken over" : "busy, gave up");
}

static void usb_stop(void) {
    if(link.usb_ok) {
        /* Synchronous: the composite HID's deinit has run when this returns. */
        furi_hal_usb_set_config(link.usb_prev, NULL);
        usb_duck_free();
        usb_duck_set_rx_callback(NULL, NULL);
    }
    link.usb_ok = false;
}

static void bt_start(void) {
    link.bt = furi_record_open(RECORD_BT);
    link.bt_status = BtStatusOff;
    link.bt_started_at = furi_get_tick();
    bt_disconnect(link.bt);
    furi_delay_ms(200); /* let the radio core drop the old connection */
    bt_keys_storage_set_storage_path(link.bt, BT_KEYS_PATH);
    /* Subscribe first: the service only reports changes, and advertising
     * starts as soon as the profile does. */
    bt_set_status_changed_callback(link.bt, bt_status_changed, NULL);
    link.profile = bt_profile_start(link.bt, ble_profile_hid, (void*)&bt_params);
    if(link.profile) furi_hal_bt_start_advertising();
    FURI_LOG_I(TAG, "Bluetooth HID %s", link.profile ? "started" : "failed to start");
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
    link.count = 0;
    link.owed_x = link.owed_y = 0;
    FURI_LOG_I(TAG, "Link stopped");
}

LinkKind link_kind(void) {
    return link.kind;
}

bool link_is_connected(void) {
    if(!link.started) return false;
    if(link.kind == LinkUsb) return link.usb_ok && usb_duck_is_connected();
    return link.profile && link.bt_status == BtStatusConnected;
}

/* Advertising or connected, from the Bt service's status messages only.
 * Never ask the HAL (furi_hal_bt_is_active and friends): the GAP layer holds
 * its state lock while the firmware's pairing dialog waits for the user, so
 * asking from the GUI thread deadlocks the dialog it is waiting for. */
static bool bt_on_air(void) {
    return link.profile &&
           (link.bt_status == BtStatusConnected || link.bt_status == BtStatusAdvertising);
}

const char* link_status(void) {
    if(link.kind == LinkUsb) {
        if(!link.usb_ok) return "USB busy";
        return usb_duck_is_connected() ? "USB" : "no USB";
    }
    if(!link.profile) return "BT failed";
    if(link.bt_status == BtStatusConnected) return "BT";
    return bt_on_air() ? "BT pairing" : "BT starting";
}

bool link_bt_off(void) {
    if(!link.started || link.kind != LinkBluetooth) return false;
    if(!link.profile) return true;
    bool grace = tick_before(link.bt_started_at + furi_ms_to_ticks(BT_START_GRACE_MS));
    return !bt_on_air() && !grace;
}

const char* link_bt_name(void) {
    static char name[24];
    const char* flipper = furi_hal_version_get_name_ptr();
    snprintf(name, sizeof(name), "%s %s", bt_params.device_name_prefix, flipper ? flipper : "");
    return name;
}

void link_bt_forget(void) {
    if(!link.started || link.kind != LinkBluetooth || !link.profile) return;
    bt_disconnect(link.bt);
    furi_delay_ms(200);
    furi_hal_bt_stop_advertising();
    bt_forget_bonded_devices(link.bt);
    furi_hal_bt_start_advertising();
    FURI_LOG_I(TAG, "Forgot every paired computer");
}

/* The outbox */

static bool queue(ReportType type, int16_t a, int16_t b) {
    if(!link_is_connected() || link.count >= OUTBOX_SIZE) return false;
    link.outbox[(link.head + link.count++) % OUTBOX_SIZE] = (Report){type, a, b};
    return true;
}

bool link_mouse_move(int8_t dx, int8_t dy) {
    return queue(ReportMove, dx, dy);
}

bool link_mouse_click(uint8_t button) {
    return queue(ReportClick, button, 0);
}

bool link_mouse_scroll(int8_t wheel) {
    return queue(ReportScroll, wheel, 0);
}

bool link_consumer_tap(uint16_t usage) {
    return queue(ReportConsumer, (int16_t)usage, 0);
}

bool link_busy(void) {
    return link.count || link.owed_x || link.owed_y;
}

static int8_t clamp8(int16_t v) {
    return v > 127 ? 127 : v < -127 ? -127 : (int8_t)v;
}

/* Over Bluetooth the firmware's report calls return false when the report
 * went out (ble_gatt_characteristic_update returns result != SUCCESS), so
 * their results can't be trusted either way; its own Remote ignores them.
 * Taking false as a failure would keep every move owed and send it again
 * with the next one, and a click would never be released. A Bluetooth
 * report counts as sent while the computer is connected. */
static bool send_move(int8_t dx, int8_t dy) {
    if(link.kind == LinkUsb) return usb_duck_mouse_move(dx, dy);
    ble_profile_hid_mouse_move(link.profile, dx, dy);
    return true;
}

static bool send(const Report* r) {
    if(link.kind == LinkUsb) {
        switch(r->type) {
        case ReportClick:
            return usb_duck_mouse_click(r->a);
        case ReportScroll:
            return usb_duck_mouse_scroll(r->a);
        case ReportConsumer:
            return usb_duck_consumer_tap((uint16_t)r->a);
        case ReportMove:
            break;
        }
        return false;
    }
    switch(r->type) {
    case ReportClick:
        ble_profile_hid_mouse_press(link.profile, r->a);
        ble_profile_hid_mouse_release(link.profile, r->a);
        break;
    case ReportScroll:
        ble_profile_hid_mouse_scroll(link.profile, r->a);
        break;
    case ReportConsumer:
        ble_profile_hid_consumer_key_press(link.profile, (uint16_t)r->a);
        ble_profile_hid_consumer_key_release(link.profile, (uint16_t)r->a);
        break;
    case ReportMove:
        break;
    }
    return true;
}

/* Moves are merged with whatever is still owed; what fails stays owed. */
static bool send_owed(void) {
    int8_t dx = clamp8(link.owed_x), dy = clamp8(link.owed_y);
    if(!send_move(dx, dy)) return false;
    link.owed_x -= dx;
    link.owed_y -= dy;
    return true;
}

void link_flush(void) {
    if(!link_is_connected()) {
        /* What the computer never got must not arrive later as a jump. */
        link.count = 0;
        link.owed_x = link.owed_y = 0;
        return;
    }
    bool ok = true;
    while(ok && link.count) {
        Report r = link.outbox[link.head];
        link.head = (link.head + 1) % OUTBOX_SIZE;
        link.count--;
        if(r.type == ReportMove) {
            link.owed_x += r.a;
            link.owed_y += r.b;
            ok = send_owed();
        } else {
            ok = send(&r);
        }
    }
    while(ok && (link.owed_x || link.owed_y)) ok = send_owed();

    if(!ok && !link.failing) FURI_LOG_W(TAG, "Report not sent; retrying with the next one");
    link.failing = !ok;
}
