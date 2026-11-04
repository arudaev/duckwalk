# Duckwalk helper (Windows)

A small program for your computer that tells Duckwalk when **you** are using your mouse or keyboard. With it running, Harold lets go of the pointer the moment you touch your own mouse, keyboard or touchpad, stays away for 30 seconds after your last input, and walks only while you are really away.

Without the helper, Duckwalk works as before: it can't see your input, so it walks on its timer whether you are there or not.

## Use it

1. Start Duckwalk on the Flipper, plugged in over USB or paired over Bluetooth.
2. Run `duckwalk-helper.exe` and leave it open (minimised is fine).
3. The Flipper shows **USB+PC** or **BT+PC** in the corner. While you work, the status line says **You're working**.

To start it with Windows, put a shortcut to it in `shell:startup` (Win+R, type `shell:startup`, Enter).

`duckwalk-helper --verbose` prints every input device it sees and whether it counts as you or as Duckwalk, plus how long you have been idle.

## What it does, exactly

- **It listens to input metadata, not content.** Windows Raw Input tells the helper that an input event happened and which device sent it. The helper never looks at which keys you press.
- **It ignores Duckwalk's own devices:** `VID_1209&PID_0001` over USB, and the Bluetooth HID device of "Harold <your Flipper's name>". Every other device counts as you. So does input made by software (remote desktop, automation tools), which has no device at all.
- **It sends one number,** the milliseconds since your last input, to the Flipper: over Duckwalk's USB data channel, or over Bluetooth to a small service of Duckwalk's own next to the mouse. It does that once a second, and straight away when you are active (at most four times a second). The format is in [`app/helper_protocol.h`](../../app/helper_protocol.h). Nothing goes anywhere else, and nothing is saved.

## Limits

- **Bluetooth needs a Visual Studio build.** The Bluetooth part uses the Windows SDK's GATT API, which MinGW lacks; a MinGW build works over USB only. The `duckwalk-helper.exe` from CI has both.
- **Windows only.** macOS and Linux versions would need their own input APIs; contributions are welcome.

## Build

You need CMake and a C compiler. Visual Studio gives USB and Bluetooth; MinGW gcc gives USB only:

```bash
cmake -S desktop/helper -B build/helper
cmake --build build/helper
```

CI builds it on Windows for every pull request and uploads `duckwalk-helper.exe`.
