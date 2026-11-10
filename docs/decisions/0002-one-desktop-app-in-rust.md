# 0002 – One desktop app for Windows, macOS and Linux, in Rust

[← Decision 0001](0001-tell-duckwalk-from-the-user.md) · [Docs home](../README.md) · [Next: Decision 0003 →](0003-trusted-install-for-free.md)

Issue #119, 6 November 2026. Tried on Windows 11 (Surface, ARM64) with Duckwalk on official firmware 1.5.1.

## Question

The PC helper (`desktop/helper`) is C on plain Win32, so it runs on Windows only. M3 adds a tray icon, start at login, Harold on the screen and data in both directions. Do we grow the C helper and write two more for macOS and Linux, or start one app for all three?

## Decision

One app, in Rust, in `desktop/app/`. It replaced the C helper once it did everything the helper did (#120).

**Why Rust:**

- One language and one build (`cargo build`) for all three systems, with CI building each on its own runner.
- The pieces we need exist as maintained libraries: `hidapi` for USB HID, `btleplug` for Bluetooth LE, `windows` for WinRT, and tray and window crates for later stories.
- A single small executable with no runtime to install, which matters for something that starts at login.
- Memory safety for a program that runs all day and parses packets from a device.

Electron or Tauri would make Harold's on-screen window easier but cost a browser engine for what is mostly a background process. Go or C# would work too, but their HID and Bluetooth libraries are thinner on macOS and Linux.

## What is shared and what stays per system

| Part | Shared | Per system |
|---|---|---|
| The protocol (`protocol.rs`) | The same 63-byte packets as `app/helper_protocol.h` | – |
| USB (`usb.rs`) | `hidapi`: vendor 0x1209, product 0xD0C4, the vendor collection 0xFF00 | Windows lists each collection on its own; macOS and Linux list the interface once |
| Bluetooth (`ble/`) | Duckwalk's service and its packet characteristic | Windows: WinRT. macOS and Linux: `btleplug` |
| Seeing your input (#120) | The idle-time logic | Raw Input on Windows, IOHIDManager on macOS, evdev on Linux |
| Harold on the screen (#123) | The drawing | Window flags; GNOME on Wayland can't float a window over others, so Harold stays in the tray there |

**Why Bluetooth differs on Windows.** `btleplug` on Windows finds only devices that advertise. A Flipper that is already connected to the computer doesn't advertise, so `btleplug` never saw Harold. Windows does list the GATT services of every paired device, which is how the C helper finds him, and the app does the same through WinRT (`GattDeviceService::GetDeviceSelectorFromUuid`). BlueZ on Linux lists paired and connected devices, so `btleplug` should find him there. macOS may hide connected devices from a scan the same way Windows does; that needs a tester with a Mac.

## Checked

- Windows 11, over USB and Bluetooth: the Flipper shows +PC and "You're working" while you work; Harold's own walks are ignored, so the idle time keeps counting while he walks.
- The tray, start at login and the goodbye on Quit, including a log-out and log-in (#121).
- The unit tests check that packets match the C helper's byte for byte.
- CI builds, lints and tests the app on Windows, macOS and Linux.

Not yet checked on a real Mac or Linux computer (#117).

## Limits

- On macOS the app needs Input Monitoring, and on Linux the `input` group; without them it reports "away", so Harold walks as he would without it.
- **The tray's event loop and Raw Input share one registration** per device type in a program. The input watcher starts after the event loop, so its registration is the one that counts.

---

[← Decision 0001](0001-tell-duckwalk-from-the-user.md) · [Docs home](../README.md) · [Next: Decision 0003 →](0003-trusted-install-for-free.md)
