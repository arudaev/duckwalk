/* Duckwalk helper for Windows: tells Duckwalk when you are using your own
 * mouse or keyboard, so Harold keeps his hands off the pointer while you
 * work and walks only when you are away.
 *
 * How: Windows Raw Input reports every mouse and keyboard event together
 * with the device it came from. Events from Duckwalk's own devices are
 * ignored; everything else counts as you. Once a second, and straight away
 * when you are active, the helper writes how long you have been idle into
 * Duckwalk's data channel (app/helper_protocol.h): over USB, or over
 * Bluetooth when Duckwalk is paired that way.
 *
 * Plain Win32 and C, no dependencies. Nothing leaves the computer except
 * that one number, and only to the Flipper. */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
/* The Bluetooth GATT API is in the Windows SDK; MinGW builds go without. */
#if defined(__has_include)
#if __has_include(<bluetoothleapis.h>)
#include <bluetoothleapis.h>
#define HAVE_BLE 1
#endif
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "../../app/helper_protocol.h"

/* Duckwalk over USB (pid.codes test IDs, see app/usb/usb_duck.h). Its data
 * channel is the third collection of that HID interface. */
#define DUCKWALK_USB      L"vid_1209&pid_0001"
#define DUCKWALK_DATA_COL L"&col03"
/* Duckwalk over Bluetooth pairs as "Harold <Flipper name>". Windows names
 * its input devices after its Bluetooth address, for example
 *   the paired device:  BTHLE\DEV_80E12780C2E2\...
 *   its mouse:          HID#{00001812-...}_80E12780C2E2&Col02#...
 * so the helper looks up the address of every paired Harold and ignores
 * input from device paths that carry it. */
#define HAROLD_NAME       L"Harold "
#define HAROLD_MAX        8
#define REFRESH_HAROLD_MS 30000

#define SEND_ACTIVE_MS 250
#define RETRY_OPEN_MS  2000

static const GUID hid_interface = {0x4D1E55B2, 0xF16F, 0x11CF, {0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30}};
#ifdef HAVE_BLE
/* Over Bluetooth: Duckwalk's own service and the characteristic the packets
 * go to (app/ble_duck.c). Windows lists each service of a paired device as a
 * device interface named after its UUID. */
static const GUID ble_service = {0x15f18352, 0x9e6d, 0x4ff4, {0x9f, 0x45, 0xcd, 0x15, 0x8a, 0x70, 0x94, 0x1c}};
static const GUID ble_packet = {0x15f18352, 0x9e6d, 0x4ff4, {0x9f, 0x45, 0xcd, 0x15, 0x8a, 0x70, 0x94, 0x1d}};
#endif

static bool verbose = false;
static HANDLE duckwalk = INVALID_HANDLE_VALUE;
static ULONGLONG user_at;
static ULONGLONG sent_at;
static ULONGLONG open_tried_at;
static unsigned long ignored = 0;
static wchar_t harold[HAROLD_MAX][16]; /* "_80e12780c2e2", lower case */
static int harold_count = 0;
static ULONGLONG harold_refreshed_at;

/* Remember which raw-input handles are Duckwalk; the oldest entry goes
 * when the cache is full. */
#define DEVICE_CACHE 32
static struct {
    HANDLE handle;
    bool duckwalk;
} devices[DEVICE_CACHE];
static int device_count = 0;

static void lowercase(wchar_t* s) {
    for(; *s; s++) *s = (wchar_t)towlower(*s);
}

static bool is_duckwalk_path(const wchar_t* path) {
    if(wcsstr(path, DUCKWALK_USB)) return true;
    for(int i = 0; i < harold_count; i++) {
        if(wcsstr(path, harold[i])) return true;
    }
    return false;
}

/* Find the Bluetooth address of every paired "Harold ..." device. */
static void refresh_harold(void) {
    HDEVINFO set = SetupDiGetClassDevsW(NULL, L"BTHLE", NULL, DIGCF_ALLCLASSES);
    if(set == INVALID_HANDLE_VALUE) return;
    int found = 0;
    SP_DEVINFO_DATA info = {.cbSize = sizeof(info)};
    for(DWORD i = 0; found < HAROLD_MAX && SetupDiEnumDeviceInfo(set, i, &info); i++) {
        wchar_t name[128] = {0}, id[256] = {0};
        if(!SetupDiGetDeviceRegistryPropertyW(set, &info, SPDRP_FRIENDLYNAME, NULL, (BYTE*)name, sizeof(name) - sizeof(wchar_t), NULL)) continue;
        if(wcsncmp(name, HAROLD_NAME, wcslen(HAROLD_NAME)) != 0) continue;
        if(!SetupDiGetDeviceInstanceIdW(set, &info, id, 255, NULL)) continue;
        lowercase(id);
        /* "bthle\dev_80e12780c2e2\..." */
        const wchar_t* dev = wcsstr(id, L"dev_");
        if(!dev || wcslen(dev) < 4 + 12) continue;
        swprintf(harold[found], 16, L"_%.12ls", dev + 4);
        if(verbose) wprintf(L"paired Duckwalk over Bluetooth: %ls (%ls)\n", name, harold[found] + 1);
        found++;
    }
    SetupDiDestroyDeviceInfoList(set);
    if(found != harold_count) device_count = 0; /* look at every device again */
    harold_count = found;
}

static bool device_is_duckwalk(HANDLE device) {
    int cached = device_count < DEVICE_CACHE ? device_count : DEVICE_CACHE;
    for(int i = 0; i < cached; i++) {
        if(devices[i].handle == device) return devices[i].duckwalk;
    }
    wchar_t name[512] = {0};
    UINT size = sizeof(name) / sizeof(name[0]);
    bool duck = false;
    if(GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, name, &size) != (UINT)-1) {
        lowercase(name);
        duck = is_duckwalk_path(name);
        if(verbose) wprintf(L"device %ls: %ls\n", duck ? L"Duckwalk (ignored)" : L"yours", name);
    } else if(verbose) {
        wprintf(L"device %p: name unknown (error %lu), counted as yours\n", (void*)device, GetLastError());
    }
    devices[device_count % DEVICE_CACHE].handle = device;
    devices[device_count % DEVICE_CACHE].duckwalk = duck;
    device_count++;
    return duck;
}

/* Duckwalk's data channel: find and open it. */

static void close_duckwalk(void) {
    if(duckwalk != INVALID_HANDLE_VALUE) CloseHandle(duckwalk);
    duckwalk = INVALID_HANDLE_VALUE;
}

static bool open_duckwalk(void) {
    HDEVINFO set = SetupDiGetClassDevsW(&hid_interface, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if(set == INVALID_HANDLE_VALUE) return false;
    SP_DEVICE_INTERFACE_DATA iface = {.cbSize = sizeof(iface)};
    for(DWORD i = 0; duckwalk == INVALID_HANDLE_VALUE && SetupDiEnumDeviceInterfaces(set, NULL, &hid_interface, i, &iface); i++) {
        DWORD size = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &iface, NULL, 0, &size, NULL);
        SP_DEVICE_INTERFACE_DETAIL_DATA_W* detail = malloc(size);
        if(!detail) break;
        detail->cbSize = sizeof(*detail);
        if(SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, size, NULL, NULL)) {
            wchar_t path[512];
            wcsncpy(path, detail->DevicePath, 511);
            path[511] = 0;
            lowercase(path);
            if(wcsstr(path, DUCKWALK_USB) && wcsstr(path, DUCKWALK_DATA_COL)) {
                duckwalk = CreateFileW(detail->DevicePath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                       OPEN_EXISTING, 0, NULL);
            }
        }
        free(detail);
    }
    SetupDiDestroyDeviceInfoList(set);
    if(duckwalk != INVALID_HANDLE_VALUE) printf("Connected to Duckwalk over USB.\n");
    return duckwalk != INVALID_HANDLE_VALUE;
}

static void send_idle(ULONGLONG now) {
    if(duckwalk == INVALID_HANDLE_VALUE) {
        if(now - open_tried_at < RETRY_OPEN_MS) return;
        open_tried_at = now;
        if(!open_duckwalk()) return;
    }
    /* Report ID, then the 63-byte packet. */
    uint8_t report[1 + HELPER_PACKET_LEN] = {4, HELPER_MAGIC, HELPER_VERSION, HELPER_MSG_IDLE};
    ULONGLONG idle = now - user_at;
    helper_put_u32(&report[4], idle > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)idle);
    DWORD written;
    if(!WriteFile(duckwalk, report, sizeof(report), &written, NULL)) {
        printf("Lost Duckwalk (unplugged, switched to Bluetooth, or closed). Waiting for it...\n");
        close_duckwalk();
        return;
    }
    sent_at = now;
}

#ifdef HAVE_BLE
/* Duckwalk over Bluetooth. A write can stall for seconds while Harold is out
 * of range, so a thread of its own does them and input keeps flowing. */

static volatile ULONGLONG ble_user_at;
static HANDLE ble_wake;
static volatile bool ble_connected = false;

static HANDLE open_ble(BTH_LE_GATT_CHARACTERISTIC* packet) {
    HANDLE service = INVALID_HANDLE_VALUE;
    HDEVINFO set = SetupDiGetClassDevsW(&ble_service, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if(set == INVALID_HANDLE_VALUE) return service;
    SP_DEVICE_INTERFACE_DATA iface = {.cbSize = sizeof(iface)};
    for(DWORD i = 0; service == INVALID_HANDLE_VALUE && SetupDiEnumDeviceInterfaces(set, NULL, &ble_service, i, &iface); i++) {
        DWORD size = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &iface, NULL, 0, &size, NULL);
        SP_DEVICE_INTERFACE_DETAIL_DATA_W* detail = malloc(size);
        if(!detail) break;
        detail->cbSize = sizeof(*detail);
        if(SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, size, NULL, NULL)) {
            service = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  NULL, OPEN_EXISTING, 0, NULL);
        }
        free(detail);
        if(service == INVALID_HANDLE_VALUE) continue;
        BTH_LE_GATT_CHARACTERISTIC chars[4];
        USHORT count = 0;
        bool found = false;
        if(SUCCEEDED(BluetoothGATTGetCharacteristics(service, NULL, 4, chars, &count, BLUETOOTH_GATT_FLAG_NONE))) {
            for(USHORT c = 0; c < count && !found; c++) {
                if(!chars[c].CharacteristicUuid.IsShortUuid &&
                   IsEqualGUID(&chars[c].CharacteristicUuid.Value.LongUuid, &ble_packet)) {
                    *packet = chars[c];
                    found = true;
                }
            }
        }
        if(!found) {
            CloseHandle(service);
            service = INVALID_HANDLE_VALUE;
        }
    }
    SetupDiDestroyDeviceInfoList(set);
    return service;
}

static DWORD WINAPI ble_thread(LPVOID arg) {
    (void)arg;
    HANDLE service = INVALID_HANDLE_VALUE;
    BTH_LE_GATT_CHARACTERISTIC packet;
    struct {
        ULONG size;
        UCHAR data[HELPER_PACKET_LEN];
    } value = {.size = HELPER_PACKET_LEN};
    for(;;) {
        WaitForSingleObject(ble_wake, HELPER_HEARTBEAT_MS);
        if(duckwalk != INVALID_HANDLE_VALUE) continue; /* USB is doing it */
        if(service == INVALID_HANDLE_VALUE) {
            service = open_ble(&packet);
            if(service == INVALID_HANDLE_VALUE) continue;
        }
        memset(value.data, 0, sizeof(value.data));
        value.data[0] = HELPER_MAGIC;
        value.data[1] = HELPER_VERSION;
        value.data[2] = HELPER_MSG_IDLE;
        ULONGLONG idle = GetTickCount64() - ble_user_at;
        helper_put_u32(&value.data[3], idle > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)idle);
        HRESULT result = BluetoothGATTSetCharacteristicValue(service, &packet, (PBTH_LE_GATT_CHARACTERISTIC_VALUE)&value,
                                                             0, BLUETOOTH_GATT_FLAG_WRITE_WITHOUT_RESPONSE);
        if(SUCCEEDED(result) != ble_connected) {
            ble_connected = SUCCEEDED(result);
            printf(ble_connected ? "Connected to Duckwalk over Bluetooth.\n"
                                 : "Lost Duckwalk over Bluetooth (out of range, or closed). Waiting for it...\n");
            fflush(stdout);
        }
        if(FAILED(result)) {
            CloseHandle(service);
            service = INVALID_HANDLE_VALUE;
            Sleep(RETRY_OPEN_MS);
        }
    }
}
#endif

/* Input */

static void on_input(HRAWINPUT input) {
    RAWINPUTHEADER header;
    UINT size = sizeof(header);
    if(GetRawInputData(input, RID_HEADER, &header, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1) return;
    /* No device means input made by software (remote desktop, automation):
     * treat it as you, it isn't Duckwalk. */
    if(header.hDevice && device_is_duckwalk(header.hDevice)) {
        ignored++;
        return;
    }
    ULONGLONG now = GetTickCount64();
    user_at = now;
#ifdef HAVE_BLE
    ble_user_at = now;
    SetEvent(ble_wake);
#endif
    if(now - sent_at >= SEND_ACTIVE_MS) send_idle(now);
}

static LRESULT CALLBACK window_proc(HWND window, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch(msg) {
    case WM_INPUT:
        on_input((HRAWINPUT)lparam);
        break;
    case WM_TIMER:
        if(GetTickCount64() - harold_refreshed_at >= REFRESH_HAROLD_MS) {
            harold_refreshed_at = GetTickCount64();
            refresh_harold();
        }
        send_idle(GetTickCount64());
        if(verbose) {
            printf("\ridle %5llu s, Duckwalk %s, %lu of its own moves ignored   ",
                   (GetTickCount64() - user_at) / 1000,
                   duckwalk != INVALID_HANDLE_VALUE ? "on USB"
#ifdef HAVE_BLE
                   : ble_connected ? "on Bluetooth"
#endif
                   : "not found",
                   ignored);
            fflush(stdout);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, msg, wparam, lparam);
}

int main(int argc, char** argv) {
    for(int i = 1; i < argc; i++) {
        if(!strcmp(argv[i], "--verbose") || !strcmp(argv[i], "-v")) {
            verbose = true;
        } else {
            printf("Usage: duckwalk-helper [--verbose]\n\n"
                   "Tells Duckwalk on a Flipper Zero when you use your own mouse or keyboard,\n"
                   "so Harold walks the pointer only while you are away. Leave it running.\n");
            return argc > 1 && strcmp(argv[i], "--help") ? 1 : 0;
        }
    }

    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSW cls = {.lpfnWndProc = window_proc, .hInstance = instance, .lpszClassName = L"DuckwalkHelper"};
    RegisterClassW(&cls);
    /* A hidden window: Raw Input with INPUTSINK needs a window to deliver to. */
    HWND window = CreateWindowExW(0, cls.lpszClassName, L"Duckwalk helper", 0, 0, 0, 0, 0, NULL, NULL, instance, NULL);
    if(!window) {
        fprintf(stderr, "Can't create the input window (error %lu).\n", GetLastError());
        return 1;
    }

    /* A precision touchpad moves the pointer through synthesised mouse input,
     * but its gestures (two-finger scroll, pinch) never show up as mouse
     * input at all. Listening to the touchpad and touch screen themselves
     * catches every finger on them. */
    RAWINPUTDEVICE wanted[] = {
        {.usUsagePage = 0x01, .usUsage = 0x02, .dwFlags = RIDEV_INPUTSINK, .hwndTarget = window}, /* mice */
        {.usUsagePage = 0x01, .usUsage = 0x06, .dwFlags = RIDEV_INPUTSINK, .hwndTarget = window}, /* keyboards */
        {.usUsagePage = 0x0D, .usUsage = 0x05, .dwFlags = RIDEV_INPUTSINK, .hwndTarget = window}, /* touchpads */
        {.usUsagePage = 0x0D, .usUsage = 0x04, .dwFlags = RIDEV_INPUTSINK, .hwndTarget = window}, /* touch screens */
    };
    if(!RegisterRawInputDevices(wanted, sizeof(wanted) / sizeof(wanted[0]), sizeof(wanted[0]))) {
        fprintf(stderr, "Can't listen to input (error %lu).\n", GetLastError());
        return 1;
    }

    user_at = GetTickCount64();
    harold_refreshed_at = user_at;
    refresh_harold();
    printf("Duckwalk helper: Harold now waits while you use your own mouse or keyboard.\n"
           "Leave this window open (or minimised). Ctrl+C stops it.\n");
    open_duckwalk();
#ifdef HAVE_BLE
    ble_user_at = user_at;
    ble_wake = CreateEventW(NULL, FALSE, FALSE, NULL);
    CreateThread(NULL, 0, ble_thread, NULL, 0, NULL);
    if(duckwalk == INVALID_HANDLE_VALUE) printf("Duckwalk isn't connected over USB; looking for it over Bluetooth too...\n");
#else
    if(duckwalk == INVALID_HANDLE_VALUE) printf("Duckwalk isn't connected over USB yet; waiting for it...\n");
#endif
    SetTimer(window, 1, HELPER_HEARTBEAT_MS, NULL);

    MSG msg;
    while(GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    close_duckwalk();
    return 0;
}
