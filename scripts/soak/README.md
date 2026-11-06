# Soak test

Tools for the 24-hour soak in [#65](https://github.com/arudaev/duckwalk/issues/65): leave Harold running for a day and check that the computer never went idle, the Flipper didn't run out of memory and Duckwalk was still going at the end.

| File | Runs on | Logs |
|---|---|---|
| `soak_pc.c` | the Windows PC | once a minute: Windows idle time, locked or not, reports from each mouse device |
| `soak_flipper.py` | the Windows PC, talking to the Flipper over USB | every 5 minutes: free heap, battery, the running app |
| `soak_report.py` | anywhere | a Markdown summary of both logs for the PR, and whether the run passed |

## Set up

```bash
gcc -O2 -o soak_pc.exe scripts/soak/soak_pc.c -luser32
python -m pip install pyserial
```

Close qFlipper: it holds the Flipper's serial port. Set Windows to turn the screen off and lock after a few minutes, so a failure shows up in the log. Set Duckwalk's Stop after to Never.

## Bluetooth soak (Wander)

1. Plug the Flipper in by USB, start Duckwalk, choose Bluetooth and Wander, and pair it.
2. Start both loggers and the [desktop app](../../desktop/app/README.md):

   ```bash
   soak_pc.exe pc-ble.csv 24
   python scripts/soak/soak_flipper.py flipper-ble.csv 24
   ```

3. During the day, put the PC to sleep and wake it, walk out of Bluetooth range and come back.
4. For the battery figure, unplug the Flipper for a few hours. The Flipper log shows `cli=absent` while it's unplugged. When you plug it back in, the report prints the charge it lost per hour.

## USB soak (Walk)

While Duckwalk's USB mode is on, the app owns the port, so the Flipper has no command line and its log reads `cli=absent` for the whole run. Read the heap around the run instead:

1. With Duckwalk closed, open the Flipper's CLI (or run `soak_flipper.py` once) and note the free heap.
2. Start Duckwalk, choose USB and Walk, and run `soak_pc.exe pc-usb.csv 24`.
3. Pull the cable and plug it back in once or twice during the day.
4. After 24 hours, check that Harold is still walking, quit Duckwalk and read the free heap again. A leak shows up as less free heap than before the run.

## Report

```bash
python scripts/soak/soak_report.py pc-ble.csv flipper-ble.csv
python scripts/soak/soak_report.py pc-usb.csv --from "2026-11-05 09:00" --to "2026-11-06 09:00"
```

The report flags:
- every minute in which the Windows idle time reached 2 minutes (`--idle-limit` changes it);
- every minute the PC was locked;
- gaps in the PC log, which mean the PC slept or the logger stopped;
- a drop of more than 4 KB in free heap;
- any reading with Duckwalk not running.

It exits with 1 when something needs a look. The device table shows who moved the pointer:
- Duckwalk over USB (`VID_1209`)
- a Bluetooth mouse
- `touchpad or software`: precision touchpads and remote desktop give Windows no device handle

Paste the output into the PR.
