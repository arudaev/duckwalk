<p align="center">
  <img src="brand/banner.png" alt="Duckwalk: a pen drawing of Harold, a yellow rubber duck, nudging a mouse pointer along a dotted path from a Flipper Zero to a laptop" width="100%">
</p>

# Duckwalk

Duckwalk turns a Flipper Zero into Harold, a rubber debugging duck who lives on your desk. When you step away, Harold keeps your computer awake by walking the mouse pointer around, never on the same path twice. With the small PC helper running, he lets go the moment you touch your own mouse or keyboard. The same Flipper is also a remote for the computer.

<p align="center">
  <img src="docs/screens/home-awake@4x.png" width="32%" alt="Harold at his desk, reading code on his laptop">
  <img src="docs/screens/home-bug-quack@4x.png" width="32%" alt="Harold quacking at a bug in the code">
  <img src="docs/screens/home-walking@4x.png" width="32%" alt="Harold walking while the pointer walks">
</p>

It works over USB or Bluetooth. The Flipper app needs nothing on the computer and no drivers; the PC helper is optional.

> **Status: in development, not released yet.** Tested on a Flipper (official firmware 1.5.1) with Windows 11, over USB and Bluetooth. Version 0.1 is being built in the open; follow the [M1 milestone](https://github.com/arudaev/duckwalk/milestone/2).

**[Read the docs](docs/README.md)**: the [user guide](docs/guide.md) with every screen, the [PC helper](desktop/helper/README.md), and for contributors the [architecture](docs/architecture.md) and [developer guide](docs/development.md).

## What Harold does

| On the Flipper | What happens on the computer |
|---|---|
| **Home** – Harold at his desk, reading code and hunting bugs. OK puts him to sleep or wakes him; holding OK makes him quack | Keep-awake on or off. In **Walk** mode the pointer takes a short, curved, randomised walk every few minutes and Harold walks along on the screen; in **Subtle** mode it moves one pixel and back |
| **Right** – Settings | Mode, USB or Bluetooth, how often, how far, work hours only, sound and vibration |
| **Left** – Remote | **Media:** play/pause, tracks, volume. **Slides:** next and back. **Mouse:** the D-pad moves the pointer, OK clicks |
| **Up** – Preview | The next walk drawn to scale before it happens; OK plays it now |
| **Down** – Bluetooth | A pairing checklist that shows which side, Flipper or computer, needs attention |
| **Back** | Leaves the app; USB and Bluetooth go back to normal |

The [user guide](docs/guide.md) shows every screen.

## How it works

Most Flipper apps that talk to a computer switch the USB port into a single mode, so a mouse jiggler and a PC monitor cannot run at the same time. Duckwalk presents one USB device with three parts: a mouse, media keys and a small data channel. All three are standard USB HID, which computers support without extra drivers; so far it has been tested on Windows 11, and macOS and Linux reports are welcome.

Teams, screen savers and lock timers decide you are away from the computer's idle timer, and any mouse input resets it. In our test the idle timer reset with every move while the data channel was busy.

Over Bluetooth, Duckwalk uses the Flipper firmware's own HID profile and pairs like any Bluetooth mouse. The PC helper uses Windows Raw Input to tell your devices from Duckwalk's and sends one number, how long you have been idle, over the USB data channel. The [architecture page](docs/architecture.md) has the diagrams.

## Install

Once version 0.1 is out, Duckwalk will be in the official Flipper Apps Catalog: install it from the Flipper mobile app or [Flipper Lab](https://lab.flipper.net/apps).

Until then, build it from source. You need Python 3.8+ and a Flipper on the official firmware.

```bash
python -m pip install --upgrade ufbt
git clone https://github.com/arudaev/duckwalk.git
cd duckwalk/app
ufbt launch      # builds and starts it on a connected Flipper (close qFlipper first)
```

## Before you use it at work

A keep-awake tool can be against your employer's rules. Duckwalk makes your computer look attended; whether that is acceptable where you work is your decision. On its own it only moves the pointer. It never types, and it clicks or presses media keys only when you use the remote.

## Contributing

Issues, testing on other computers, and art are welcome. Start with [CONTRIBUTING.md](CONTRIBUTING.md).

## Credits

- Harold is the duck from [VOYAGER // 56K](https://arudaev.github.io), a small sailing game by the same author.
- The USB interface follows the structure of the Flipper Zero firmware's own HID driver ([flipperdevices/flipperzero-firmware](https://github.com/flipperdevices/flipperzero-firmware), GPL-3.0).

## Licence

[GNU General Public License v3.0](LICENSE).
