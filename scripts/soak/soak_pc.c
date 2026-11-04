/* Soak logger for the PC side (Windows).
 *
 * Once a minute it writes one CSV line: the Windows idle time, whether the PC
 * is locked, and for every mouse device how many raw reports Windows got and
 * their summed movement. Devices are listed by name when they first report and
 * when they are plugged in or removed, so Duckwalk's USB device (VID_1209) and
 * its Bluetooth mouse can be told apart from your own mouse.
 *
 * Build:  gcc -O2 -o soak_pc.exe soak_pc.c -luser32
 *         (or: cl /O2 soak_pc.c user32.lib)
 * Run:    soak_pc pc.csv 24      (file, hours; the file is appended to)
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_DEV 32

static FILE* out;
static ULONGLONG until;
static HANDLE devs[MAX_DEV];
static long count[MAX_DEV], sx[MAX_DEV], sy[MAX_DEV];
static int ndev;

static void stamp(char* buf, size_t len) {
    SYSTEMTIME t;
    GetLocalTime(&t);
    snprintf(buf, len, "%04d-%02d-%02d %02d:%02d:%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
}

static void device_name(HANDLE dev, char* name, size_t len) {
    if(dev == NULL) {
        /* No device handle: a precision touchpad (Windows synthesises its
         * mouse input) or software such as SendInput or remote desktop. */
        snprintf(name, len, "no-device");
        return;
    }
    wchar_t w[512] = {0};
    UINT size = sizeof(w) / sizeof(w[0]);
    if(GetRawInputDeviceInfoW(dev, RIDI_DEVICENAME, w, &size) == (UINT)-1 ||
       !WideCharToMultiByte(CP_UTF8, 0, w, -1, name, (int)len, NULL, NULL))
        snprintf(name, len, "unknown");
}

static int slot(HANDLE dev) {
    for(int i = 0; i < ndev; i++)
        if(devs[i] == dev) return i;
    if(ndev == MAX_DEV) return MAX_DEV - 1;
    char name[600], t[40];
    device_name(dev, name, sizeof(name));
    stamp(t, sizeof(t));
    fprintf(out, "# %s dev%d %s\n", t, ndev, name);
    devs[ndev] = dev;
    return ndev++;
}

static int locked(void) {
    HDESK d = OpenInputDesktop(0, FALSE, DESKTOP_SWITCHDESKTOP);
    if(!d) return 1;
    BOOL ok = SwitchDesktop(d);
    CloseDesktop(d);
    return !ok;
}

static void flush_minute(void) {
    LASTINPUTINFO li = {sizeof(li), 0};
    GetLastInputInfo(&li);
    char t[40];
    stamp(t, sizeof(t));
    fprintf(out, "%s,idle_s=%lu,locked=%d", t, (GetTickCount() - li.dwTime) / 1000, locked());
    for(int i = 0; i < ndev; i++) {
        fprintf(out, ",dev%d=%ld/%ld/%ld", i, count[i], sx[i], sy[i]);
        count[i] = sx[i] = sy[i] = 0;
    }
    fprintf(out, "\n");
    fflush(out);
}

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    if(m == WM_INPUT) {
        RAWINPUT ri;
        UINT size = sizeof(ri);
        if(GetRawInputData((HRAWINPUT)lp, RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER)) != (UINT)-1 &&
           ri.header.dwType == RIM_TYPEMOUSE) {
            int i = slot(ri.header.hDevice);
            count[i]++;
            sx[i] += ri.data.mouse.lLastX;
            sy[i] += ri.data.mouse.lLastY;
        }
    } else if(m == WM_INPUT_DEVICE_CHANGE) {
        char name[600], t[40];
        device_name((HANDLE)lp, name, sizeof(name));
        stamp(t, sizeof(t));
        fprintf(out, "# %s %s %s\n", t, wp == GIDC_ARRIVAL ? "added" : "removed", name);
        fflush(out);
    } else if(m == WM_TIMER) {
        flush_minute();
        if(GetTickCount64() > until) PostQuitMessage(0);
    }
    return DefWindowProcW(h, m, wp, lp);
}

int main(int argc, char** argv) {
    if(argc < 2) {
        fprintf(stderr, "usage: soak_pc out.csv [hours]\n");
        return 2;
    }
    out = fopen(argv[1], "a");
    if(!out) {
        perror(argv[1]);
        return 1;
    }
    double hours = argc > 2 ? atof(argv[2]) : 24;
    until = GetTickCount64() + (ULONGLONG)(hours * 3600000.0);

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = proc;
    wc.lpszClassName = L"duckwalk_soak";
    RegisterClassW(&wc);
    HWND h = CreateWindowW(L"duckwalk_soak", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
    RAWINPUTDEVICE rid = {1, 2, RIDEV_INPUTSINK | RIDEV_DEVNOTIFY, h}; /* generic desktop, mouse */
    if(!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
        fprintf(stderr, "RegisterRawInputDevices failed: %lu\n", GetLastError());
        return 1;
    }
    SetTimer(h, 1, 60000, NULL);
    printf("Logging to %s for %.1f h. Ctrl+C stops it.\n", argv[1], hours);

    MSG msg;
    while(GetMessageW(&msg, NULL, 0, 0) > 0) DispatchMessageW(&msg);
    fclose(out);
    return 0;
}
