---
name: duckwalk
description: "Use when the user wants their computer kept awake (or no longer kept awake) during long unattended work, or mentions Duckwalk, Harold, a Flipper Zero mouse jiggler or keep-awake. Covers installing Duckwalk's desktop app on Windows, macOS and Linux, adding its MCP server, and the tools status, wake, sleep and set_mode."
metadata:
  author: arudaev
  version: "0.1.0"
---

# Duckwalk

Duckwalk is a Flipper Zero app: Harold, a rubber debugging duck, keeps a computer awake by moving the mouse pointer a little while nobody is there, and puts it back where it was. The optional desktop app sits in the tray, tells the Flipper when the user is working (so Harold never moves the pointer while they do), and lets agents control him through an MCP server.

## When to use it

- Before long unattended work (a build, a test run, a download, an agent task that runs for a while): **wake** Harold so the computer doesn't sleep or lock.
- When that work is done, or the user leaves for the day: **sleep**, so the computer locks as usual.
- When the user asks what Harold is doing: **status**.

Don't wake Harold without a reason the user would agree with: keeping a work computer awake can be against an employer's rules, and that's the user's call.

## Tools (MCP server `duckwalk`)

| Tool | Does |
|---|---|
| `status` | Connection (`usb`, `bluetooth`, `none`), whether keep-awake is on, the mode, whether Harold is walking the pointer or waiting because the user is working, seconds since the user's last own input |
| `wake` | Keep-awake on, in Harold's usual mode (like pressing OK on the Flipper) |
| `sleep` | Keep-awake off |
| `set_mode` | `subtle` (one pixel now and then), `walk` (a short walk every few minutes) or `wander` (the pointer drifts without stopping). Wakes him in that mode and keeps it |

Check `status` first. If `connection` is `none`, the Flipper isn't connected: ask the user to open Duckwalk on it (USB or Bluetooth) rather than retrying. If a tool says the desktop app isn't running, ask the user to start it; it lives in the tray.

## Installing the desktop app

Only with the user's agreement; it starts at login afterwards (its tray menu turns that off).

- **Windows:** download `duckwalk-desktop-windows-x64.exe` (or `-arm64.exe`) from https://github.com/arudaev/duckwalk/releases and run it once; it installs itself to `%LOCALAPPDATA%\Programs\Duckwalk` and the Start menu.
- **macOS:** `brew install --HEAD arudaev/tap/duckwalk`, then `duckwalk`. macOS asks for Input Monitoring the first time.
- **Linux:** install `libudev-dev libdbus-1-dev pkg-config libgtk-3-dev libxdo-dev libayatana-appindicator3-dev`, then `cargo install --git https://github.com/arudaev/duckwalk duckwalk`, then `duckwalk`. The user must be in the `input` group.

The Flipper app itself comes from the Flipper Apps Catalog or the releases page; Duckwalk needs the official Flipper firmware 1.5 or newer.

## Adding the MCP server

The server is the desktop app started as `duckwalk mcp` (stdio). It talks to the running tray app over a local socket and never opens a network port.

- **Claude Code:** `claude mcp add duckwalk -- duckwalk mcp` (on Windows, use the full path: `%LOCALAPPDATA%\Programs\Duckwalk\duckwalk.exe`)
- **Codex:** in `~/.codex/config.toml`:
  ```toml
  [mcp_servers.duckwalk]
  command = "duckwalk"
  args = ["mcp"]
  ```
- **Others:** any MCP client that starts a stdio server: command `duckwalk`, argument `mcp`.

More: https://github.com/arudaev/duckwalk/blob/main/desktop/app/README.md
