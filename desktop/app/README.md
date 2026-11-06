# Duckwalk desktop app

One app for Windows, macOS and Linux that will replace the [PC helper](../helper/README.md). Why Rust, and what stays per system: [decision 0002](../../docs/decisions/0002-one-desktop-app-in-rust.md).

**It sees your input** on Windows, macOS and Linux: your own mouse, keyboard, touchpad, touch screen and pen, never Duckwalk's own moves. It tells Harold how long you have been idle, over USB or Bluetooth.

**On macOS, allow Input Monitoring.** On first start the app asks for it; allow it in **System Settings > Privacy & Security > Input Monitoring** and start the app again. It reads only that an input happened and from which device, never which keys. Without it Harold walks on his timer.

**On Linux, join the `input` group first.** The app reads your devices from `/dev/input/`, which only that group may do:

```bash
sudo usermod -aG input $USER
```

Then log out and in again. Without it the app says it can't read any input device, and Harold walks on his timer.

## In the tray

Harold sits in the tray (the menu bar on macOS), in colour while he's connected and grey while the app looks for him. His menu says how he's connected and whether you're working, and has:

- **Start at login:** on after the first start. Windows uses the Run entry for your user, macOS a Launch Agent, Linux an autostart file in `~/.config/autostart/`.
- **Quit Duckwalk:** tells the Flipper goodbye, so it drops **+PC** at once instead of after 5 seconds.

There is no window. Only one copy runs at a time.

## Run it

```bash
cd desktop/app
cargo run --release -- --verbose
```

The Flipper shows **USB+PC** or **BT+PC** in the corner, and **You're working** while you use the computer. `--verbose` lists every input device as yours or Duckwalk's. To check the link without touching anything, `--pretend-working` says you are working all the time, and the Flipper's status line shows **You're working**.

## Build

You need Rust (stable). On Linux, also `libudev-dev`, `libdbus-1-dev`, `pkg-config`, and for the tray `libgtk-3-dev`, `libxdo-dev` and `libayatana-appindicator3-dev`.

```bash
cargo test
cargo build --release
```

CI builds it on Windows, macOS and Linux for every pull request and uploads `duckwalk-desktop-<system>`.

| File | Does |
|---|---|
| `src/main.rs` | Sends an idle packet every second and straight away when you are active, over USB when Duckwalk is plugged in, otherwise over Bluetooth; a goodbye on Quit |
| `src/tray.rs`, `src/status.rs` | Harold in the tray, and the line his menu shows |
| `src/login.rs` | Start at login |
| `src/input/` | Sees your input: Raw Input on Windows, IOHIDManager on macOS, evdev on Linux |
| `src/protocol.rs` | The packets, the same as `app/helper_protocol.h` |
| `src/usb.rs` | Finds Duckwalk's data channel with `hidapi` |
| `src/ble/` | Finds Duckwalk's Bluetooth service: WinRT on Windows, `btleplug` on macOS and Linux |
