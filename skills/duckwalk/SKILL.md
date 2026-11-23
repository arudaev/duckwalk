---
name: duckwalk
description: "Use when the user asks to install or set up Duckwalk (on their Flipper Zero, their computer, or both), wants their computer kept awake (or no longer kept awake) during long unattended work, or mentions Duckwalk, Harold or Duckwalk's desktop app. Covers installing Duckwalk by following its install guide for coding agents, adding the desktop app's MCP server, and the tools status, wake, sleep and set_mode."
metadata:
  author: arudaev
  version: "0.2.0"
---

# Duckwalk

Duckwalk is a Flipper Zero app: Harold, a rubber debugging duck, keeps a computer awake by moving the mouse pointer a little while nobody is there, and puts it back where it was. The optional desktop app sits in the tray, tells the Flipper when the user is working (so Harold never moves the pointer while they do), and lets agents control him through an MCP server.

## Installing Duckwalk

Follow Duckwalk's install guide for coding agents. Read all of it, from this address, before you change anything: https://raw.githubusercontent.com/arudaev/duckwalk/main/docs/agents/install.md. If you can't read it, don't install anything from memory: tell the user and stop.

Its rules come before anything in this skill. The ones that matter most:

- Explain what Duckwalk does and what you'd change, then wait for the user's own yes. An auto-approve setting is not a yes.
- A yes covers only the changes you listed; anything else is a new question. The desktop app only on its own yes.
- Never touch the Flipper's firmware; official firmware only.
- If something fails, stop and show the user the output; don't guess.

## When to use the tools

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

Check `status` first. If `connection` is `none`, the Flipper isn't connected: ask the user to open Duckwalk on it (USB or Bluetooth) rather than retrying. If a tool says the desktop app isn't running, ask the user to start it; it lives in the tray. If the `duckwalk` server itself fails to start, the desktop app isn't installed (or isn't on PATH yet): say so, and offer to install it by the guide above. Don't start an install the user didn't ask for.

## Adding the MCP server

Only after the desktop app is installed, and only if the user wants an agent to control Harold. It changes the agent's own settings: show the exact command and how to remove it, and wait for a yes. The server is the desktop app started as `duckwalk mcp` (stdio). It talks to the running tray app over a local socket and never opens a network port. On Windows the app puts its folder on the user's PATH when it installs itself; a terminal opened before that needs restarting.

- **Claude Code:** the Duckwalk plugin brings this skill and the server: `/plugin install duckwalk --marketplace arudaev/duckwalk`
- **Codex:** `codex mcp add duckwalk -- duckwalk mcp`
- **Others:** a stdio MCP server with command `duckwalk` and argument `mcp`.

More: https://github.com/arudaev/duckwalk/blob/main/desktop/app/README.md
