# 04 · Architecture

[← Contributing](../CONTRIBUTING.md) · [Docs home](README.md) · [Next: Developer guide →](development.md)

How the Duckwalk app is put together. Read this before changing anything that runs on more than one thread, or anything that talks to the computer.

## The pieces

```mermaid
flowchart LR
    subgraph Flipper["Flipper app (app/)"]
        main["duckwalk.c<br/>main loop, screens, app lock"]
        screens["home.c · settings_screen.c · remote.c<br/>preview.c · pairing.c · welcome.c"]
        ka["keep_awake.c<br/>when to move"]
        walk["walk.c<br/>where to move"]
        settings["settings.c<br/>settings.txt on the SD card"]
        helperf["helper.c<br/>is the user working?"]
        link["link.c<br/>USB or Bluetooth, report outbox"]
        usb["usb/usb_duck.c<br/>composite USB HID"]
        ble["firmware BLE HID profile"]
    end
    subgraph PC["Computer"]
        os["Windows, macOS, Linux<br/>(built-in HID drivers)"]
        helper["desktop/app<br/>desktop app (tray)"]
    end

    main --> screens
    main --> ka --> walk
    main --> settings
    main --> helperf
    screens --> link
    ka --> link
    link --> usb
    link --> ble
    usb -- "mouse, media keys" --> os
    ble -- "mouse, media keys" --> os
    helper -- "idle time, data channel" --> usb
    usb -- "packets" --> helperf
```

| File | Owns |
|---|---|
| `duckwalk.c` | The app's life: start-up, the main loop, which screen is up, slow work outside the lock, shutdown |
| `duckwalk.h` | The app state every screen shares, and the rules for touching it |
| `home.c` and the other screens | Drawing (`*_draw`), buttons (`*_input`) and animation (`*_tick`) for one screen each |
| `keep_awake.c` | When to nudge or walk; plays a walk one 20 ms step at a time |
| `walk.c` | Plans a walk: a smooth curve through random waypoints that ends where it started |
| `link.c` | The connection: starts USB or Bluetooth, queues reports and sends them, carries dropped moves into the next report |
| `usb/usb_duck.c` | One USB HID interface with three parts: mouse, media keys and a 63-byte data channel |
| `ble_duck.c` | The Bluetooth profile: the firmware's HID mouse and media keys, plus a one-characteristic service the desktop app writes to |
| `helper.c`, `helper_protocol.h` | Packets from the desktop app; the protocol both sides share (`desktop/app/src/protocol.rs` mirrors it) |
| `settings.c` | Reading and writing `settings.txt`, defaults line by line |
| `util.h` | The log tag and tick helpers that survive the counter wrapping |

## Threads and the app lock

Four threads touch Duckwalk. Each has one job. The rule that keeps them apart: **nothing holds the app lock while it waits for something slow.**

```mermaid
sequenceDiagram
    participant In as Input service
    participant Main as Main loop (duckwalk.c)
    participant GUI as GUI thread
    participant Link as USB interrupt / Bt service

    In->>Main: button event (queue, never blocks)
    Note over Main: lock: input, ticks, state changes,<br/>reports go into the outbox
    Main->>Main: unlock
    Main->>Link: send the outbox, switch connection, save settings
    GUI->>Main: draw: lock for at most 25 ms, read state, unlock
    Link-->>Main: status / data-channel packets (one word each)
```

- **Main loop:** handles input, runs every screen's tick and keep-awake, all under the lock. Then, without the lock, it does the slow work the screens asked for (`app->pending`) and sends the queued reports (`link_flush`).
- **GUI thread:** calls the draw callback, which waits at most 25 ms for the lock and skips a frame if it can't get it. Draw functions only read state.
- **Input service:** puts button events in a queue and never waits.
- **USB interrupt and Bt service:** store a connection status or a data-channel packet; the other threads only read them.

The loop wakes every 20 ms while something moves (a walk, the Remote's mouse, queued reports) and every 100 ms otherwise, so the Flipper can idle.

**Why so strict:** when a computer pairs over Bluetooth, the firmware shows a "confirm the code" dialog. While that dialog waits, the Bluetooth layer holds its own lock. An app that waits on the Bluetooth layer from the GUI thread, or makes the GUI wait for it, stops the dialog from ever being drawn, and the Flipper freezes until it is rebooted. Duckwalk did exactly that once. That's why the Bluetooth state comes only from the Bt service's status messages, never from `furi_hal_bt_*` queries, and why drawing never blocks.

## Screens and buttons

```mermaid
stateDiagram-v2
    [*] --> Welcome: first launch
    [*] --> Home
    Welcome --> Home: USB
    Welcome --> Pairing: Bluetooth
    Home --> Settings: Right
    Home --> Remote: Left
    Home --> Preview: Up
    Home --> Pairing: Down
    Settings --> Home: Back
    Settings --> Pairing: Back, Bluetooth not connected
    Remote --> Home: Back from the menu
    Preview --> Home: Back
    Pairing --> Home: Back, or the pointer test passed
    Home --> [*]: Back
```

## Keep-awake and walks

```mermaid
flowchart TD
    wait["Waiting<br/>(random 30 s to 4 min)"] -->|time is up| mode{Mode}
    mode -->|Subtle| nudge["1 px out and back"] --> wait
    mode -->|Walk| play["Play the planned walk<br/>one step every 20 ms"]
    mode -->|Wander| wander["Wander: steer for a random spot,<br/>a new one before it's reached"]
    wander -->|mode off, work hours over, leaving the app| home["Glide back to the start"] --> wait
    wander -->|"user active (desktop app)"| stop
    play -->|last step| plan["Plan the next walk"] --> wait
    play -->|"user active (desktop app)"| stop["Let go of the pointer"] --> hold
    wait -->|user active| hold["Hold: nothing moves<br/>until Resume after (30 s by default)<br/>after the user's last input"] -->|user away| wait
```

A walk is planned before it plays, so Preview shows the path that will really play. If a report doesn't get through, its movement is added to the next one (`link.c`), so a walk always ends where it started. A wander isn't planned: `wander.c` works out each step from where the cursor is, turning a little at a time so it moves in arcs, and keeps count of what it sent, so going home lands exactly on the start.

## The data channel

Over USB, Duckwalk's HID interface has a third collection: 63-byte vendor reports in both directions. The desktop app writes the user's idle time into it once a second, and says goodbye when it quits so the Flipper drops +PC at once; the format is in [`app/helper_protocol.h`](../app/helper_protocol.h). Over Bluetooth the same packets go to a service of Duckwalk's own (`ble_duck.c`): one write-only characteristic, 63 bytes, writable only by the paired computer. The firmware's serial service would have done, but its two 486-byte buffers don't fit beside the HID service. The profile wraps the firmware's HID profile, so its mouse and media functions work unchanged, and Windows picks the new service up without pairing again.

How the desktop app tells Duckwalk's own input from yours is written up in [decision 0001](decisions/0001-tell-duckwalk-from-the-user.md) and [decision 0002](decisions/0002-one-desktop-app-in-rust.md).

---

[← Contributing](../CONTRIBUTING.md) · [Docs home](README.md) · [Next: Developer guide →](development.md)
