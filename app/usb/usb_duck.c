/* Duckwalk's USB interface: mouse, media keys and a data channel at once.
 * Structure follows the firmware's targets/f7/furi_hal/furi_hal_usb_hid.c
 * (GPL-3.0), extended with a vendor-defined collection. */

#include "usb_duck.h"

#include <usb.h>
#include <usb_hid.h>
#include <usb_std.h>
#include <usbd_core.h>
#include <hid_usage_desktop.h>
#include <hid_usage_button.h>
#include <hid_usage_consumer.h>

#define DUCK_EP_IN  0x81
#define DUCK_EP_OUT 0x01
#define DUCK_EP_SZ  64
#define DUCK_INTERVAL 2

/* Values from furi_hal_usb_i.h, which is not part of the SDK. */
#define DUCK_EP0_SIZE  8
#define DUCK_STR_MANUF 1
#define DUCK_STR_PROD  2

struct DuckIntfDescriptor {
    struct usb_interface_descriptor hid;
    struct usb_hid_descriptor hid_desc;
    struct usb_endpoint_descriptor hid_ep_in;
    struct usb_endpoint_descriptor hid_ep_out;
} FURI_PACKED;

struct DuckConfigDescriptor {
    struct usb_config_descriptor config;
    struct DuckIntfDescriptor intf_0;
} FURI_PACKED;

static const uint8_t duck_report_desc[] = {
    // clang-format off
    HID_USAGE_PAGE(HID_PAGE_DESKTOP),
    HID_USAGE(HID_DESKTOP_MOUSE),
    HID_COLLECTION(HID_APPLICATION_COLLECTION),
        HID_USAGE(HID_DESKTOP_POINTER),
        HID_COLLECTION(HID_PHYSICAL_COLLECTION),
            HID_REPORT_ID(USB_DUCK_REPORT_MOUSE),
            HID_USAGE_PAGE(HID_PAGE_BUTTON),
            HID_USAGE_MINIMUM(1),
            HID_USAGE_MAXIMUM(3),
            HID_LOGICAL_MINIMUM(0),
            HID_LOGICAL_MAXIMUM(1),
            HID_REPORT_COUNT(3),
            HID_REPORT_SIZE(1),
            HID_INPUT(HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),
            HID_REPORT_SIZE(1),
            HID_REPORT_COUNT(5),
            HID_INPUT(HID_IOF_CONSTANT | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),
            HID_USAGE_PAGE(HID_PAGE_DESKTOP),
            HID_USAGE(HID_DESKTOP_X),
            HID_USAGE(HID_DESKTOP_Y),
            HID_USAGE(HID_DESKTOP_WHEEL),
            HID_LOGICAL_MINIMUM(-127),
            HID_LOGICAL_MAXIMUM(127),
            HID_REPORT_SIZE(8),
            HID_REPORT_COUNT(3),
            HID_INPUT(HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_RELATIVE),
        HID_END_COLLECTION,
    HID_END_COLLECTION,

    HID_USAGE_PAGE(HID_PAGE_CONSUMER),
    HID_USAGE(HID_CONSUMER_CONTROL),
    HID_COLLECTION(HID_APPLICATION_COLLECTION),
        HID_REPORT_ID(USB_DUCK_REPORT_CONSUMER),
        HID_LOGICAL_MINIMUM(0),
        HID_RI_LOGICAL_MAXIMUM(16, 0x3FF),
        HID_USAGE_MINIMUM(0),
        HID_RI_USAGE_MAXIMUM(16, 0x3FF),
        HID_REPORT_COUNT(1),
        HID_REPORT_SIZE(16),
        HID_INPUT(HID_IOF_DATA | HID_IOF_ARRAY | HID_IOF_ABSOLUTE),
    HID_END_COLLECTION,

    HID_RI_USAGE_PAGE(16, 0xFF00),
    HID_USAGE(0x01),
    HID_COLLECTION(HID_APPLICATION_COLLECTION),
        HID_REPORT_ID(USB_DUCK_REPORT_VENDOR),
        HID_LOGICAL_MINIMUM(0),
        HID_RI_LOGICAL_MAXIMUM(16, 0xFF),
        HID_REPORT_SIZE(8),
        HID_REPORT_COUNT(USB_DUCK_VENDOR_LEN),
        HID_USAGE(0x02),
        HID_INPUT(HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),
        HID_REPORT_COUNT(USB_DUCK_VENDOR_LEN),
        HID_USAGE(0x03),
        HID_OUTPUT(HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),
    HID_END_COLLECTION,
    // clang-format on
};

static const struct usb_string_descriptor duck_manuf_desc = USB_STRING_DESC("arudaev");
static const struct usb_string_descriptor duck_prod_desc = USB_STRING_DESC("Duckwalk");

static struct usb_device_descriptor duck_device_desc = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass = USB_SUBCLASS_NONE,
    .bDeviceProtocol = USB_PROTO_NONE,
    .bMaxPacketSize0 = DUCK_EP0_SIZE,
    .idVendor = USB_DUCK_VID,
    .idProduct = USB_DUCK_PID,
    .bcdDevice = VERSION_BCD(0, 1, 0),
    .iManufacturer = DUCK_STR_MANUF,
    .iProduct = DUCK_STR_PROD,
    .iSerialNumber = NO_DESCRIPTOR,
    .bNumConfigurations = 1,
};

static const struct DuckConfigDescriptor duck_cfg_desc = {
    .config =
        {
            .bLength = sizeof(struct usb_config_descriptor),
            .bDescriptorType = USB_DTYPE_CONFIGURATION,
            .wTotalLength = sizeof(struct DuckConfigDescriptor),
            .bNumInterfaces = 1,
            .bConfigurationValue = 1,
            .iConfiguration = NO_DESCRIPTOR,
            .bmAttributes = USB_CFG_ATTR_RESERVED | USB_CFG_ATTR_SELFPOWERED,
            .bMaxPower = USB_CFG_POWER_MA(500),
        },
    .intf_0 =
        {
            .hid =
                {
                    .bLength = sizeof(struct usb_interface_descriptor),
                    .bDescriptorType = USB_DTYPE_INTERFACE,
                    .bInterfaceNumber = 0,
                    .bAlternateSetting = 0,
                    .bNumEndpoints = 2,
                    .bInterfaceClass = USB_CLASS_HID,
                    .bInterfaceSubClass = USB_HID_SUBCLASS_NONBOOT,
                    .bInterfaceProtocol = USB_HID_PROTO_NONBOOT,
                    .iInterface = NO_DESCRIPTOR,
                },
            .hid_desc =
                {
                    .bLength = sizeof(struct usb_hid_descriptor),
                    .bDescriptorType = USB_DTYPE_HID,
                    .bcdHID = VERSION_BCD(1, 0, 0),
                    .bCountryCode = USB_HID_COUNTRY_NONE,
                    .bNumDescriptors = 1,
                    .bDescriptorType0 = USB_DTYPE_HID_REPORT,
                    .wDescriptorLength0 = sizeof(duck_report_desc),
                },
            .hid_ep_in =
                {
                    .bLength = sizeof(struct usb_endpoint_descriptor),
                    .bDescriptorType = USB_DTYPE_ENDPOINT,
                    .bEndpointAddress = DUCK_EP_IN,
                    .bmAttributes = USB_EPTYPE_INTERRUPT,
                    .wMaxPacketSize = DUCK_EP_SZ,
                    .bInterval = DUCK_INTERVAL,
                },
            .hid_ep_out =
                {
                    .bLength = sizeof(struct usb_endpoint_descriptor),
                    .bDescriptorType = USB_DTYPE_ENDPOINT,
                    .bEndpointAddress = DUCK_EP_OUT,
                    .bmAttributes = USB_EPTYPE_INTERRUPT,
                    .wMaxPacketSize = DUCK_EP_SZ,
                    .bInterval = DUCK_INTERVAL,
                },
        },
};

static void duck_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx);
static void duck_deinit(usbd_device* dev);
static void duck_on_wakeup(usbd_device* dev);
static void duck_on_suspend(usbd_device* dev);

FuriHalUsbInterface usb_duck = {
    .init = duck_init,
    .deinit = duck_deinit,
    .wakeup = duck_on_wakeup,
    .suspend = duck_on_suspend,

    .dev_descr = &duck_device_desc,

    .str_manuf_descr = (void*)&duck_manuf_desc,
    .str_prod_descr = (void*)&duck_prod_desc,
    .str_serial_descr = NULL,

    .cfg_descr = (void*)&duck_cfg_desc,
};

static usbd_device* usb_dev;
static FuriSemaphore* tx_semaphore = NULL;
static UsbDuckRxCallback rx_callback = NULL;
static void* rx_ctx = NULL;
static volatile bool connected = false;

static usbd_respond duck_ep_config(usbd_device* dev, uint8_t cfg);
static usbd_respond duck_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback);

void usb_duck_set_rx_callback(UsbDuckRxCallback callback, void* ctx) {
    rx_ctx = ctx;
    rx_callback = callback;
}

bool usb_duck_is_connected(void) {
    return connected;
}

/* Every IN report shares one endpoint; the semaphore is released when the
 * previous packet has been transmitted. */
static bool duck_send(const void* report, uint16_t len) {
    if(tx_semaphore == NULL || !connected) return false;
    if(furi_semaphore_acquire(tx_semaphore, DUCK_INTERVAL * 4) != FuriStatusOk) return false;
    if(!connected) return false;
    usbd_ep_write(usb_dev, DUCK_EP_IN, report, len);
    return true;
}

bool usb_duck_mouse_move(int8_t dx, int8_t dy) {
    uint8_t report[5] = {USB_DUCK_REPORT_MOUSE, 0, (uint8_t)dx, (uint8_t)dy, 0};
    return duck_send(report, sizeof(report));
}

bool usb_duck_consumer_tap(uint16_t usage) {
    uint8_t press[3] = {USB_DUCK_REPORT_CONSUMER, usage & 0xFF, usage >> 8};
    uint8_t release[3] = {USB_DUCK_REPORT_CONSUMER, 0, 0};
    if(!duck_send(press, sizeof(press))) return false;
    return duck_send(release, sizeof(release));
}

bool usb_duck_vendor_send(const uint8_t* data, size_t len) {
    uint8_t report[1 + USB_DUCK_VENDOR_LEN] = {USB_DUCK_REPORT_VENDOR};
    if(len > USB_DUCK_VENDOR_LEN) len = USB_DUCK_VENDOR_LEN;
    memcpy(&report[1], data, len);
    return duck_send(report, sizeof(report));
}

static void duck_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    UNUSED(intf);
    UNUSED(ctx);
    if(tx_semaphore == NULL) tx_semaphore = furi_semaphore_alloc(1, 1);
    usb_dev = dev;
    usbd_reg_config(dev, duck_ep_config);
    usbd_reg_control(dev, duck_control);
    usbd_connect(dev, true);
}

static void duck_deinit(usbd_device* dev) {
    usbd_reg_config(dev, NULL);
    usbd_reg_control(dev, NULL);
    connected = false;
}

static void duck_on_wakeup(usbd_device* dev) {
    UNUSED(dev);
    connected = true;
}

static void duck_on_suspend(usbd_device* dev) {
    UNUSED(dev);
    if(connected) {
        connected = false;
        furi_semaphore_release(tx_semaphore);
    }
}

static void duck_txrx_ep_callback(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(dev);
    if(event == usbd_evt_eptx) {
        furi_semaphore_release(tx_semaphore);
        return;
    }
    uint8_t buf[DUCK_EP_SZ];
    int32_t len = usbd_ep_read(usb_dev, ep, buf, sizeof(buf));
    if(len < 1 || buf[0] != USB_DUCK_REPORT_VENDOR || rx_callback == NULL) return;
    UsbDuckVendorPacket packet = {0};
    memcpy(packet.data, &buf[1], MIN((size_t)(len - 1), sizeof(packet.data)));
    rx_callback(&packet, rx_ctx);
}

static usbd_respond duck_ep_config(usbd_device* dev, uint8_t cfg) {
    switch(cfg) {
    case 0:
        usbd_ep_deconfig(dev, DUCK_EP_OUT);
        usbd_ep_deconfig(dev, DUCK_EP_IN);
        usbd_reg_endpoint(dev, DUCK_EP_OUT, 0);
        usbd_reg_endpoint(dev, DUCK_EP_IN, 0);
        return usbd_ack;
    case 1:
        usbd_ep_config(dev, DUCK_EP_IN, USB_EPTYPE_INTERRUPT, DUCK_EP_SZ);
        usbd_ep_config(dev, DUCK_EP_OUT, USB_EPTYPE_INTERRUPT, DUCK_EP_SZ);
        usbd_reg_endpoint(dev, DUCK_EP_IN, duck_txrx_ep_callback);
        usbd_reg_endpoint(dev, DUCK_EP_OUT, duck_txrx_ep_callback);
        usbd_ep_write(dev, DUCK_EP_IN, 0, 0);
        return usbd_ack;
    default:
        return usbd_fail;
    }
}

static usbd_respond duck_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    UNUSED(callback);
    if(((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) ==
           (USB_REQ_INTERFACE | USB_REQ_CLASS) &&
       req->wIndex == 0) {
        switch(req->bRequest) {
        case USB_HID_SETIDLE:
            return usbd_ack;
        default:
            return usbd_fail;
        }
    }
    if(((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) ==
           (USB_REQ_INTERFACE | USB_REQ_STANDARD) &&
       req->wIndex == 0 && req->bRequest == USB_STD_GET_DESCRIPTOR) {
        switch(req->wValue >> 8) {
        case USB_DTYPE_HID:
            dev->status.data_ptr = (uint8_t*)&(duck_cfg_desc.intf_0.hid_desc);
            dev->status.data_count = sizeof(duck_cfg_desc.intf_0.hid_desc);
            return usbd_ack;
        case USB_DTYPE_HID_REPORT:
            dev->status.data_ptr = (uint8_t*)duck_report_desc;
            dev->status.data_count = sizeof(duck_report_desc);
            return usbd_ack;
        default:
            return usbd_fail;
        }
    }
    return usbd_fail;
}
