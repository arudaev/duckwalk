# Duckwalk helper (Windows)

A small program for your computer that tells Duckwalk when **you** are using your mouse or keyboard. With it running, Harold lets go of the pointer the moment you touch your own mouse, keyboard or touchpad, stays away for 30 seconds after your last input, and walks only while you are really away.

Without the helper, Duckwalk works as before: it can't see your input, so it walks on its timer whether you are there or not.

## Use it

1. Start Duckwalk on the Flipper with **Connection: USB** and plug it in.
2. Run `duckwalk-helper.exe` and leave it open (minimised is fine).
3. The Flipper shows **USB+PC** in the corner. While you work, the status line says **You're working**.

To start it with Windows, put a shortcut to it in `shell:startup` (Win+R, type `shell:startup`, Enter).

`duckwalk-helper --verbose` prints every input device it sees and whether it counts as you or as Duckwalk, plus how long you have been idle.

## What it does, exactly

- **It listens to input metadata, not content.** Windows Raw Input tells the helper that an input event happened and which device sent it. The helper never looks at which keys you press.
- **It ignores Duckwalk's own devices:** `VID_1209&PID_0001` over USB, and the Bluetooth HID device of "Harold <your Flipper's name>". Every other device counts as you. So does input made by software (remote desktop, automation tools), which has no device at all.
- **It sends one number,** the milliseconds since your last input, to the Flipper over Duckwalk's USB data channel. It does that once a second, and straight away when you are active (at most four times a second). The format is in [`app/helper_protocol.h`](../../app/helper_protocol.h). Nothing goes anywhere else, and nothing is saved.

## Limits

- **USB only for now.** Over Bluetooth there is no data channel yet; that needs a Bluetooth profile with a serial service next to HID (issue #12).
- **Windows only.** macOS and Linux versions would need their own input APIs; contributions are welcome.

## Build

You need CMake and a C compiler (MinGW gcc or Visual Studio):

```bash
cmake -S desktop/helper -B build/helper
cmake --build build/helper
```

CI builds it on Windows for every pull request and uploads `duckwalk-helper.exe`.
