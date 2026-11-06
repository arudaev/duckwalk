# Duckwalk desktop app

One app for Windows, macOS and Linux that will replace the [PC helper](../helper/README.md). Why Rust, and what stays per system: [decision 0002](../../docs/decisions/0002-one-desktop-app-in-rust.md).

**On Windows it does what the helper does:** it sees your own mouse, keyboard, touchpad, touch screen and pen, ignores Duckwalk's own moves, and tells Harold how long you have been idle, over USB or Bluetooth. On macOS and Linux it can't see your input yet (#120), so it tells Harold you are away and he walks on his timer.

## Run it

```bash
cd desktop/app
cargo run --release -- --verbose
```

The Flipper shows **USB+PC** or **BT+PC** in the corner, and **You're working** while you use the computer. `--verbose` lists every input device as yours or Duckwalk's. To check the link without touching anything, `--pretend-working` says you are working all the time, and the Flipper's status line shows **You're working**.

## Build

You need Rust (stable). On Linux, also `libudev-dev`, `libdbus-1-dev` and `pkg-config`.

```bash
cargo test
cargo build --release
```

CI builds it on Windows, macOS and Linux for every pull request and uploads `duckwalk-desktop-<system>`.

| File | Does |
|---|---|
| `src/main.rs` | Sends an idle packet every second and straight away when you are active, over USB when Duckwalk is plugged in, otherwise over Bluetooth |
| `src/input/` | Sees your input: Raw Input on Windows; macOS and Linux to come |
| `src/protocol.rs` | The packets, the same as `app/helper_protocol.h` |
| `src/usb.rs` | Finds Duckwalk's data channel with `hidapi` |
| `src/ble/` | Finds Duckwalk's Bluetooth service: WinRT on Windows, `btleplug` on macOS and Linux |
