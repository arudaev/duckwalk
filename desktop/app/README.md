# 02 · Desktop app

[← User guide](../../docs/guide.md) · [Docs home](../../docs/README.md) · [Next: Contributing →](../../CONTRIBUTING.md)

A small app for Windows, macOS and Linux that tells Duckwalk when **you** are using your mouse or keyboard. With it running, Harold lets go of the pointer the moment you touch your own mouse, keyboard, touchpad, touch screen or pen, stays away for 30 seconds after your last input (**Resume after** in Duckwalk's Settings), and walks only while you are really away.

On Windows it also tells Harold where the pointer is: **Wander** then stays in the middle of the window you're in, clear of its title bar, its edges and the taskbar, and comes back to the exact pixel it set off from, and a **Walk** never runs off the screen.

It is optional. Without it, Duckwalk can't see your input and walks on its timer whether you are there or not; the guide says [how to manage without it](../../docs/guide.md#without-the-desktop-app).

## Use it

To install it on Windows, macOS or Linux, follow [Install, part 2](../../docs/install.md#part-2-the-desktop-app-optional); macOS asks for Input Monitoring, and Linux needs the `input` group. Start Duckwalk on the Flipper, plugged in over USB or paired over Bluetooth, then start the app. There is no window: Harold appears in the tray (the menu bar on macOS) and does what he does on the Flipper: he stands while awake, waddles while he walks the pointer, shuts his eyes while asleep, opens his bill when he quacks, and stays grey while there's no Flipper.

Harold's menu in the tray says how he's connected and whether you're working, and has:

- **Start at login:** on after the first start. Windows uses the Run entry for your user, macOS a Launch Agent, Linux an autostart file in `~/.config/autostart/`.
- **Quit Duckwalk:** tells the Flipper goodbye, so it drops **+PC** at once.

Only one copy runs at a time. On Windows a newer download updates the installed copy; quit the running one from Harold's menu first. To remove it: [Install, Remove it](../../docs/install.md#remove-it).

**Tested on Windows 11** over USB and Bluetooth. The macOS and Linux builds are new; reports are welcome.

## For coding agents

A coding agent can check on Harold and switch keep-awake on and off, for example around a long build. Everything runs on your computer: no account, no sign-in, no network.

| Agent | Set it up |
|---|---|
| **Claude Code** | `/plugin install duckwalk --marketplace arudaev/duckwalk` adds the skill and the MCP server in one go. On Claude Code before 2.1.275: `/plugin marketplace add arudaev/duckwalk`, then `/plugin install duckwalk@duckwalk` |
| **Codex** | `codex mcp add duckwalk -- duckwalk mcp` for the MCP server, and `npx skills add arudaev/duckwalk` for the skill |
| **Others** | Any MCP client: a stdio server, command `duckwalk`, argument `mcp`. The skill: `npx skills add arudaev/duckwalk` |

Then ask your agent to set Duckwalk up: the skill sends it to the [install guide for coding agents](../../docs/agents/install.md), which has it explain everything first and install only with your OK. On Windows the app puts its folder on your PATH, so `duckwalk` works by name in new terminals.

| Tool | Does |
|---|---|
| `status` | Connection, keep-awake on or off, mode, walking or waiting while you work, your idle time |
| `wake` / `sleep` | Keep-awake on or off, like OK on the Flipper |
| `set_mode` | `subtle`, `walk` or `wander` |

`duckwalk mcp` talks to the running tray app over a local socket (a named pipe on Windows), never over the network. Without the tray app or a connected Flipper, the tools say so.

## What it does, exactly

- **It notices that you used a device, not what you did with it.** It never looks at which keys you press or where you move the pointer. Windows Raw Input, macOS IOHIDManager and Linux evdev each tell the app that an input happened and which device sent it.
- **Touch screens and pens count too.** On Windows, taps and drags that the system turns into mouse input carry a mark of their own, which a low-level mouse hook spots; it looks only at the mark.
- **It ignores Duckwalk's own devices:** over USB by their IDs (`VID_1209&PID_D0C4`), over Bluetooth by the paired "Harold <your Flipper's name>". Every other device counts as you. On Windows, so does input made by software (remote desktop, automation tools), which has no device at all.
- **It sends one number,** the milliseconds since your last input, to the Flipper: over Duckwalk's USB data channel, or over Bluetooth to a small service of Duckwalk's own next to the mouse. It does that once a second, and straight away when you are active (at most four times a second). On Windows it also sends where the pointer and the active window are, so Wander stays inside the window. The format is in [`app/helper_protocol.h`](../../app/helper_protocol.h). Nothing goes anywhere else, and nothing is saved.

Why one app in Rust, and what stays per system: [decision 0002](../../docs/decisions/0002-one-desktop-app-in-rust.md).

## Run it from source

```bash
cd desktop/app
cargo run --release -- --verbose --no-install
```

`--verbose` lists every input device as yours or Duckwalk's and prints what it sends; started from a terminal it prints there. Add `--no-install` so a build from source runs where it is instead of installing itself. `--pretend-working` says you are working all the time, to check the link without touching anything: the Flipper's status line shows **You're working**.

## Build

You need Rust (stable). On Linux, also `libudev-dev`, `libdbus-1-dev`, `pkg-config`, and for the tray `libgtk-3-dev` and `libayatana-appindicator3-dev`.

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
| `src/control.rs`, `src/mcp.rs` | The local control socket, and `duckwalk mcp` on top of it |
| `src/install.rs`, `build.rs`, `assets/` | Windows: installs itself and adds the Start menu entry; Harold's icon in the exe (drawn by `brand/tools/make_icon.py`) and his tray frames (`brand/tools/make_tray.py`) |
| `src/input/` | Sees your input: Raw Input on Windows, IOHIDManager on macOS, evdev on Linux |
| `src/protocol.rs` | The packets the app sends, the same as `app/helper_protocol.h` |
| `src/flipper.rs` | The packets Duckwalk sends back: its state, each walk, a quack, closing; `--verbose` logs them |
| `src/usb.rs` | Finds Duckwalk's data channel with `hidapi` |
| `src/ble/` | Finds Duckwalk's Bluetooth service: WinRT on Windows, `btleplug` on macOS and Linux |

---

[← User guide](../../docs/guide.md) · [Docs home](../../docs/README.md) · [Next: Contributing →](../../CONTRIBUTING.md)
