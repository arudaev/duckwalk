# Duckwalk docs

Start here. Every page about Duckwalk is linked from this one.

<p align="center">
  <img src="screens/home-awake@4x.png" width="32%" alt="Harold at his desk, reading code on his laptop">
  <img src="screens/home-bug-quack@4x.png" width="32%" alt="Harold quacking at a bug in the code">
  <img src="screens/pairing-ask@4x.png" width="32%" alt="The Bluetooth pairing checklist">
</p>

## I want to use Duckwalk

| | |
|---|---|
| [**User guide**](guide.md) | Every screen and button, connecting over USB or Bluetooth, and what Harold is doing |
| [**PC helper**](../desktop/helper/README.md) | The small Windows program that makes Harold stop while you work |
| [README](../README.md) | What Duckwalk is, installing it, and the honest limits |

## I want to work on Duckwalk

| | |
|---|---|
| [**Contributing**](../CONTRIBUTING.md) | Set up in five minutes, how work flows, the rules that keep `main` releasable |
| [**Architecture**](architecture.md) | How the app is put together: files, threads, the link to the computer, the data channel |
| [**Developer guide**](development.md) | Build, run on a Flipper, debug over the CLI, host tests, the screen simulator, Bluetooth |
| [Screen simulator](../sim/README.md) | Render every screen on your computer, no Flipper needed |
| [Issues, branches and stacked PRs](contributing/README.md) | Labels, milestones, commit and PR conventions |
| [Project board](contributing/project-board.md) | The board's views, fields and automation |
| [Decisions](decisions/) | Spike results and why things are the way they are |

## Where things are

```
app/            the Flipper app (C, built with ufbt)
  usb/          the composite USB device: mouse, media keys, data channel
  images/       Harold's sprites (made by brand/tools/make_brand.py)
desktop/helper/ the PC helper for Windows (C, Win32)
tests/          host tests for the app's logic
sim/            the screen simulator
brand/          banner, sprite sources and the tool that draws the sprites
docs/           these pages; screens/ holds the simulator's renders
```

## The plan

| Milestone | What it brings | |
|---|---|---|
| **M1** v0.1 | The first release: Harold, Walk, Subtle and Wander keep-awake over USB or Bluetooth, Settings with Stop after, Remote, Preview, the connection check, the PC helper over USB or Bluetooth; Apps Catalog listing | [milestone](https://github.com/arudaev/duckwalk/milestone/2) |
| **M3** v0.3 | The desktop side: Wander that knows the screen edges, then Harold on the computer screen | [milestone](https://github.com/arudaev/duckwalk/milestone/4) |
| **M4** | Harold's extra jobs | [milestone](https://github.com/arudaev/duckwalk/milestone/5) |

The [project board](https://github.com/users/arudaev/projects/9) shows what is being worked on right now.
