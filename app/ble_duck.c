#include "ble_duck.h"
#include "helper_protocol.h"

#include <furi.h>
#include <extra_profiles/hid_profile.h>
#include <furi_ble/event_dispatcher.h>
#include <furi_ble/gatt.h>
#include <ble/core/ble_defs.h>

/* The helper's service: one characteristic it writes packets to. The
 * firmware's serial service would do, but its two 486-byte buffers don't fit
 * beside the HID service. UUIDs are Duckwalk's own, little-endian:
 * 15f18352-9e6d-4ff4-9f45-cd158a70941c (service) and ...941d (packets). */
static const Service_UUID_t service_uuid = {
    .Service_UUID_128 =
        {0x1c, 0x94, 0x70, 0x8a, 0x15, 0xcd, 0x45, 0x9f, 0xf4, 0x4f, 0x6d, 0x9e, 0x52, 0x83, 0xf1, 0x15}};

static const BleGattCharacteristicParams packet_char = {
    .name = "Helper packet",
    .data_prop_type = FlipperGattCharacteristicDataFixed,
    .data.fixed.length = HELPER_PACKET_LEN,
    .uuid.Char_UUID_128 =
        {0x1d, 0x94, 0x70, 0x8a, 0x15, 0xcd, 0x45, 0x9f, 0xf4, 0x4f, 0x6d, 0x9e, 0x52, 0x83, 0xf1, 0x15},
    .uuid_type = UUID_TYPE_128,
    .char_properties = CHAR_PROP_WRITE | CHAR_PROP_WRITE_WITHOUT_RESP,
    /* Only the paired computer may write. */
    .security_permissions = ATTR_PERMISSION_AUTHEN_WRITE,
    .gatt_evt_mask = GATT_NOTIFY_ATTRIBUTE_WRITE,
    .is_variable = CHAR_VALUE_LEN_VARIABLE,
};

/* The stack's event layout, which the SDK doesn't export. */
#define HCI_VENDOR_EVENT        0xFF
#define ACI_GATT_ATTR_MODIFIED  0x0C01

typedef struct {
    uint8_t type;
    uint8_t evt;
    uint8_t plen;
    uint16_t ecode;
    uint8_t data[];
} FURI_PACKED VendorEvent;

static struct {
    uint16_t svc_handle;
    BleGattCharacteristicInstance packet;
    GapSvcEventHandler* handler;
    bool on_air;
    /* A long write can arrive in pieces. */
    uint8_t buffer[HELPER_PACKET_LEN];
    BleDuckDataCallback callback;
    void* ctx;
} duck;

/* Bluetooth thread. */
static BleEventAckStatus on_event(void* event, void* ctx) {
    UNUSED(ctx);
    const VendorEvent* e = event;
    if(e->evt != HCI_VENDOR_EVENT || e->ecode != ACI_GATT_ATTR_MODIFIED) return BleEventNotAck;
    const aci_gatt_attribute_modified_event_rp0* m = (const void*)e->data;
    if(m->Attr_Handle != duck.packet.handle + 1) return BleEventNotAck;

    if(m->Offset + m->Attr_Data_Length <= HELPER_PACKET_LEN) {
        memcpy(duck.buffer + m->Offset, m->Attr_Data, m->Attr_Data_Length);
        BleDuckDataCallback callback = duck.callback;
        if(m->Offset + m->Attr_Data_Length == HELPER_PACKET_LEN && callback) {
            callback(duck.buffer, duck.ctx);
        }
    }
    return BleEventAckFlowEnable;
}

/* The firmware's own HID profile, so its mouse and media functions work on
 * what this returns, plus the helper's service. */
static FuriHalBleProfileBase* duck_start(FuriHalBleProfileParams params) {
    FuriHalBleProfileBase* profile = ble_profile_hid->start(params);
    /* The dispatcher insists on a context. */
    duck.handler = ble_event_dispatcher_register_svc_handler(on_event, &duck);
    duck.on_air = ble_gatt_service_add(UUID_TYPE_128, &service_uuid, PRIMARY_SERVICE, 4, &duck.svc_handle);
    if(duck.on_air) ble_gatt_characteristic_init(duck.svc_handle, &packet_char, &duck.packet);
    FURI_LOG_I(TAG, "Helper channel %s", duck.on_air ? "on air" : "failed to start");
    return profile;
}

static void duck_get_gap_config(GapConfig* config, FuriHalBleProfileParams params) {
    ble_profile_hid->get_gap_config(config, params);
}

static const FuriHalBleProfileTemplate duck_template = {
    .start = duck_start,
    /* Never called: the profile carries the HID template, so the firmware
     * stops it through that, and ble_duck_stop takes the helper's service
     * down first. */
    .stop = NULL,
    .get_gap_config = duck_get_gap_config,
};

const FuriHalBleProfileTemplate* ble_duck = &duck_template;

void ble_duck_set_data_callback(BleDuckDataCallback callback, void* ctx) {
    duck.callback = NULL;
    duck.ctx = ctx;
    duck.callback = callback;
}

/* Must run before the firmware resets the stack, which checks that every
 * service handler is gone. */
void ble_duck_stop(void) {
    if(duck.handler) {
        ble_event_dispatcher_unregister_svc_handler(duck.handler);
        duck.handler = NULL;
    }
    if(duck.on_air) {
        ble_gatt_characteristic_delete(duck.svc_handle, &duck.packet);
        ble_gatt_service_delete(duck.svc_handle);
        duck.on_air = false;
    }
}
