# 0001 – Telling Duckwalk's input apart from the user's

[← Brand](../brand.md) · [Docs home](../README.md)

Spike for issue #14, 3 November 2026. Windows 11 (Surface, ARM64), Duckwalk on official firmware 1.5.1.

## Question

Duckwalk moves the pointer through a real mouse device, so the system idle timer (`GetLastInputInfo`) resets on Duckwalk's moves and the user's alike. Can a program on the computer see only the user's own input, so Harold stops while someone works?

## What we found

**Raw Input tells devices apart.** With `RegisterRawInputDevices` (mouse and keyboard, `RIDEV_INPUTSINK`), every `WM_INPUT` carries the handle of the device that sent it, and `GetRawInputDeviceInfo(RIDI_DEVICENAME)` turns that into the device's path. Input made by software (`SendInput`, remote desktop, automation tools) arrives with no device handle at all.

**Duckwalk's devices have recognisable paths:**

| Connection | Device paths Windows creates | How to recognise them |
|---|---|---|
| USB | `HID\VID_1209&PID_0001&COL01` (mouse), `&COL02` (media keys), `&COL03` (data channel) | The vendor and product ID |
| Bluetooth | `HID\{00001812-0000-1000-8000-00805F9B34FB}_80E12780C2E2&COL01` (keyboard), `&COL02` (mouse), `&COL03` (media keys) | The Bluetooth address. The firmware's HID profile has no PnP ID, so there is no vendor ID in the path |

The paired Bluetooth device itself is `BTHLE\DEV_80E12780C2E2\...` with the friendly name "Harold Rgudalap". So the address can be found from the name, without asking the Flipper.

**`GetLastInputInfo` can't do this.** It reports the last input from any source, including Duckwalk. It stays useful for measuring keep-awake from outside, not for deciding when to stop.

## Checked on the computer

- The helper (`desktop/helper`) finds the paired "Harold Rgudalap" and its address on start-up and every 30 s.
- Input injected with `SendInput` (no device) resets the helper's idle time to 0 s.

Still to check with Duckwalk running: Harold's own walks over USB and over Bluetooth show up as ignored, and a real mouse during a walk stops it. That check is in the helper's pull request.

## Decision

Go. A small helper on the computer, in C on plain Win32, watches Raw Input, ignores Duckwalk's devices and sends the user's idle time to the Flipper over the USB data channel once a second (protocol in `app/helper_protocol.h`). The Flipper holds walks and nudges for 30 s after the user's last own input and stops a walk in progress at once.

## Limits

- **Bluetooth needs a data channel first.** The firmware's HID profile has no serial service, so over Bluetooth the helper has nothing to send through. A custom profile with HID and serial is the open half of spike #12.
- **Other systems need their own input APIs:** macOS (IOHIDManager or CGEventTap with device info) and Linux (evdev). Not started.
- **Software input counts as the user.** Remote desktop sessions and automation tools look like someone at the computer. That is the safe side: Harold waits.

---

[← Brand](../brand.md) · [Docs home](../README.md)
