# 05 · Developer guide

[← Architecture](architecture.md) · [Docs home](README.md) · [Next: Screen simulator →](../sim/README.md)

Everything you need to change Duckwalk and see the result: on a Flipper, on your computer without one, and in CI.

## Build and run on a Flipper

You need Python 3.8+ (for [ufbt](https://github.com/flipperdevices/flipperzero-ufbt)) and a Flipper on the official firmware.

```bash
python -m pip install --upgrade ufbt
cd app
ufbt            # builds dist/duckwalk.fap
ufbt launch     # copies it to the Flipper and starts it
```

`ufbt launch` talks to the Flipper over its USB serial port, so:

- **Close qFlipper and any Flipper Lab tab first;** they hold the port.
- **In USB mode Duckwalk owns the USB port,** so the serial port is gone while it runs. Press Back on the Flipper before launching again. In Bluetooth mode the port stays free.
- **The Flipper won't close a running Duckwalk remotely** ("has to be closed manually"). Press Back on the Flipper.
- `ufbt launch` reports a `ClearCommError` right after a successful start in USB mode. That's the app taking over the port, not a failure.

## Debug on the Flipper

Duckwalk logs with the tag `Duckwalk`. Open the Flipper's command line over USB (Bluetooth mode, or before starting the app):

```bash
ufbt cli
```

| Command | Shows |
|---|---|
| `log debug` | Everything the firmware and Duckwalk log, live. Ctrl+C stops |
| `top` | Every thread, its state and stack. A frozen screen with `GuiSrv` idle means something blocks drawing |
| `loader info` | Which app is running |
| `input send back short` | Presses a button (`up`, `down`, `left`, `right`, `ok`, `back`; `press`, `short`, `long`, `release`) |
| `storage read /ext/apps_data/duckwalk/settings.txt` | The saved settings |

There is no step-through debugger without an ST-Link or the WiFi devboard; logging and `top` go a long way.

## Without a Flipper

**Host tests:** the walk planner, the settings file and the helper protocol, built for your computer against small stand-ins for the firmware (`tests/shim/`):

```bash
cmake -S tests -B build/tests
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure
```

**Screen simulator:** every screen as a PNG, drawn by the app's own code with the firmware's u8g2 and fonts. See [sim/README.md](../sim/README.md). After changing a screen, render it and copy the result into `docs/screens/` so the docs stay current.

## The desktop app

```bash
cd desktop/app
cargo test
cargo run --release -- --verbose --no-install
```

Rust (stable); on Linux also `libudev-dev`, `libdbus-1-dev`, `pkg-config`, `libgtk-3-dev`, `libxdo-dev` and `libayatana-appindicator3-dev`. `--verbose` lists every input device as yours or Duckwalk's; `--pretend-working` tells Harold you are working all the time, to check the link. See [desktop/app/README.md](../desktop/app/README.md).

## Bluetooth

- **Is Harold on air?** A phone app such as nRF Connect lists "Harold <Flipper name>" advertising the HID service (`0x1812`). If you only see "Flipper <name>" with service `0x3082`, another app (PC Health, Bluetooth Remote) or the Flipper's own serial profile has the radio, and Duckwalk isn't in Bluetooth mode.
- **Pairing:** Windows pairs with numeric comparison: the Flipper's own "Verify code" dialog asks you to confirm. Never call `furi_hal_bt_*` state queries from Duckwalk, and never let drawing block; see [Architecture](architecture.md#threads-and-the-app-lock).
- **Starting over:** hold OK on the pairing checklist, then remove Harold in the computer's Bluetooth settings.

## CI

Every pull request runs:

| Check | What |
|---|---|
| Build (release SDK), Build (rc SDK) | `ufbt` builds the `.fap` against both SDKs (an Apps Catalog requirement) |
| Host tests | `tests/` with CMake and CTest |
| Screens | The simulator renders every screen and fails if text runs off it; the PNGs are uploaded as `duckwalk-screens` |
| Desktop app | Formats, lints, tests and builds `desktop/app` on Windows, macOS and Linux, and uploads each build |
| Text hygiene | No invisible characters, no em dashes, no stock phrasing (`scripts/check_text.py`) |
| PR conventions | Conventional Commits title, a linked issue, labels by path |

## Releasing

1. Raise `fap_version` in `app/application.fam` and add the version to `app/docs/changelog.md`. The catalog wants `major.minor`, higher than the last one it has.
2. Write the release notes in `docs/releases/v<version>.md` and merge both through a pull request.
3. Optional, before tagging: **Actions > Release > Run workflow** does a dry run that builds everything and releases nothing.
4. The author tags `main`, for example `git tag v0.1 && git push origin v0.1`.
5. The Release workflow checks that the tag matches `fap_version`, builds `duckwalk.fap` and the desktop app for Windows (x64, ARM64), macOS (Apple silicon, Intel) and Linux (x64), and leaves a draft release with them attached.
6. The author reads the draft and publishes it, then updates the catalog manifest's `commit_sha` (issue #10).

---

[← Architecture](architecture.md) · [Docs home](README.md) · [Next: Screen simulator →](../sim/README.md)
