# 02 · Desktop app

[← User guide](../../docs/guide.md) · [Docs home](../../docs/README.md) · [Next: Contributing →](../../CONTRIBUTING.md)

A small app for Windows, macOS and Linux that tells Duckwalk when **you** are using your mouse or keyboard. With it running, Harold lets go of the pointer the moment you touch your own mouse, keyboard, touchpad, touch screen or pen, stays away for 30 seconds after your last input (**Resume after** in Duckwalk's Settings), and walks only while you are really away.

It is optional. Without it, Duckwalk can't see your input and walks on its timer whether you are there or not; the guide says [how to manage without it](../../docs/guide.md#without-the-desktop-app).

## Use it

1. Get the app for your computer:
   - **Windows:** download it from the [releases](https://github.com/arudaev/duckwalk/releases).
   - **macOS:** with [Homebrew](https://brew.sh), which builds it on your Mac, so macOS doesn't flag it:
     ```bash
     brew install --HEAD arudaev/tap/duckwalk
     ```
     (Plain `brew install arudaev/tap/duckwalk` once v0.1 is out.)
   - **Linux:** with Rust, after installing the libraries listed under [Build](#build):
     ```bash
     cargo install --git https://github.com/arudaev/duckwalk duckwalk
     ```
     Or download the binary from the releases.
2. Start Duckwalk on the Flipper, plugged in over USB or paired over Bluetooth.
3. Start it: open the download on Windows, run `duckwalk` on macOS and Linux. There is no window: Harold appears in the tray (the menu bar on macOS), in colour once he's connected.
   - **Windows** may warn about an unknown app: **More info**, then **Run anyway**. The app then installs itself: it copies itself to `%LOCALAPPDATA%\Programs\Duckwalk`, adds **Duckwalk** to the Start menu (Windows Search finds it there) and runs from that copy, so you can delete the download. A newer download updates it the same way; quit the running one from Harold's menu first.
   - **macOS and Linux:** Homebrew and cargo put it where it can stay. A Linux binary from the releases: move it to such a folder first, since it starts from there at every login, and make it executable (`chmod +x`).
4. The Flipper shows **USB+PC** or **BT+PC** in the corner. While you work, its status line says **You're working**.

Harold's menu in the tray says how he's connected and whether you're working, and has:

- **Start at login:** on after the first start. Windows uses the Run entry for your user, macOS a Launch Agent, Linux an autostart file in `~/.config/autostart/`.
- **Quit Duckwalk:** tells the Flipper goodbye, so it drops **+PC** at once.

Only one copy runs at a time.

**To remove it:** untick **Start at login** in Harold's menu, choose **Quit Duckwalk**, then remove the app: on Windows, delete the folder `%LOCALAPPDATA%\Programs\Duckwalk` and the Start menu entry **Duckwalk**; on macOS, `brew uninstall duckwalk`; installed with cargo, `cargo uninstall duckwalk`.

**Tested on Windows 11** over USB and Bluetooth. The macOS and Linux builds are new; reports are welcome.

### macOS: allow Input Monitoring

On first start the app asks for it. Allow it in **System Settings > Privacy & Security > Input Monitoring**, then start the app again. Without it Harold walks on his timer.

### Linux: join the `input` group

The app reads your devices from `/dev/input/`, which only that group may do:

```bash
sudo usermod -aG input $USER
```

Then log out and in again. Without it the app says it can't read any input device, and Harold walks on his timer.

## For coding agents

A coding agent (Claude Code, Codex and others) can check on Harold and switch keep-awake on and off, for example around a long build. The desktop app is its own MCP server: started as `duckwalk mcp`, it talks to the running tray app over a local socket (a named pipe on Windows), never over the network.

| Tool | Does |
|---|---|
| `status` | Connection, keep-awake on or off, mode, walking or waiting while you work, your idle time |
| `wake` / `sleep` | Keep-awake on or off, like OK on the Flipper |
| `set_mode` | `subtle`, `walk` or `wander` |

Add it to **Claude Code**:

```bash
claude mcp add duckwalk -- duckwalk mcp
```

On Windows, give the full path: `%LOCALAPPDATA%\Programs\Duckwalk\duckwalk.exe mcp`. For **Codex**, add to `~/.codex/config.toml`:

```toml
[mcp_servers.duckwalk]
command = "duckwalk"
args = ["mcp"]
```

The agent skill in [`skills/duckwalk`](../../skills/duckwalk/SKILL.md) tells an agent when to use these and how to install Duckwalk; add it with `npx skills add arudaev/duckwalk`.

## What it does, exactly

- **It notices that you used a device, not what you did with it.** It never looks at which keys you press or where you move the pointer. Windows Raw Input, macOS IOHIDManager and Linux evdev each tell the app that an input happened and which device sent it.
- **Touch screens and pens count too.** On Windows, taps and drags that the system turns into mouse input carry a mark of their own, which a low-level mouse hook spots; it looks only at the mark.
- **It ignores Duckwalk's own devices:** over USB by their IDs (`VID_1209&PID_0001`), over Bluetooth by the paired "Harold <your Flipper's name>". Every other device counts as you. On Windows, so does input made by software (remote desktop, automation tools), which has no device at all.
- **It sends one number,** the milliseconds since your last input, to the Flipper: over Duckwalk's USB data channel, or over Bluetooth to a small service of Duckwalk's own next to the mouse. It does that once a second, and straight away when you are active (at most four times a second). The format is in [`app/helper_protocol.h`](../../app/helper_protocol.h). Nothing goes anywhere else, and nothing is saved.

Why one app in Rust, and what stays per system: [decision 0002](../../docs/decisions/0002-one-desktop-app-in-rust.md).

## Run it from source

```bash
cd desktop/app
cargo run --release -- --verbose --no-install
```

`--verbose` lists every input device as yours or Duckwalk's and prints what it sends; started from a terminal it prints there. Add `--no-install` so a build from source runs where it is instead of installing itself. `--pretend-working` says you are working all the time, to check the link without touching anything: the Flipper's status line shows **You're working**.

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
| `src/control.rs`, `src/mcp.rs` | The local control socket, and `duckwalk mcp` on top of it |
| `src/install.rs`, `build.rs`, `assets/` | Windows: installs itself and adds the Start menu entry; Harold's icon in the exe and the tray (drawn by `brand/tools/make_icon.py`) |
| `src/input/` | Sees your input: Raw Input on Windows, IOHIDManager on macOS, evdev on Linux |
| `src/protocol.rs` | The packets the app sends, the same as `app/helper_protocol.h` |
| `src/flipper.rs` | The packets Duckwalk sends back: its state, each walk, a quack, closing; `--verbose` logs them |
| `src/usb.rs` | Finds Duckwalk's data channel with `hidapi` |
| `src/ble/` | Finds Duckwalk's Bluetooth service: WinRT on Windows, `btleplug` on macOS and Linux |

---

[← User guide](../../docs/guide.md) · [Docs home](../../docs/README.md) · [Next: Contributing →](../../CONTRIBUTING.md)
