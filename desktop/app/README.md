# Duckwalk desktop app

One app for Windows, macOS and Linux that will replace the [PC helper](../helper/README.md). Why Rust, and what stays per system: [decision 0002](../../docs/decisions/0002-one-desktop-app-in-rust.md).

**It is a skeleton for now.** It finds Harold over USB or Bluetooth and sends him the same packets as the helper, but it can't see your input yet (#120), so it tells him you are away. For Harold to wait while you work, keep using the helper until then.

## Run it

```bash
cd desktop/app
cargo run --release -- --verbose
```

The Flipper shows **USB+PC** or **BT+PC** in the corner. To check the link, `--pretend-working` says you are working all the time, and the Flipper's status line shows **You're working**.

## Build

You need Rust (stable). On Linux, also `libudev-dev`, `libdbus-1-dev` and `pkg-config`.

```bash
cargo test
cargo build --release
```

CI builds it on Windows, macOS and Linux for every pull request and uploads `duckwalk-desktop-<system>`.

| File | Does |
|---|---|
| `src/main.rs` | Sends an idle packet every second, over USB when Duckwalk is plugged in, otherwise over Bluetooth |
| `src/protocol.rs` | The packets, the same as `app/helper_protocol.h` |
| `src/usb.rs` | Finds Duckwalk's data channel with `hidapi` |
| `src/ble/` | Finds Duckwalk's Bluetooth service: WinRT on Windows, `btleplug` on macOS and Linux |
