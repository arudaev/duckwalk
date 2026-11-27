# Efficiency budget

[Docs home](README.md)

The desktop app runs all day on work laptops, so it has to cost nothing you'd notice: never a reason to close it. This page sets what it may use, and how that's checked. A release over budget waits.

## The budget

| | At rest | While you work |
|---|---|---|
| **Private memory** | 20 MB | 20 MB |
| **CPU,** % of one core | 2 % | 4 % |
| **Wake-ups** per second | 250 | 550 |
| **Battery** | Task Manager's **Power usage** says **Very low** | the same |

- **At rest:** the app runs, the Flipper is connected, and nobody touches the computer.
- **While you work:** the script moves the pointer one pixel and back, ten times a second. A real mouse reports 125 to 1000 times a second, so this is a fixed load to compare releases with, not a measure of your own day.
- **The reference machine:** Windows 11 on ARM64, 12 logical CPUs, the native ARM64 build. Wake-ups grow with the number of CPUs (the app's async runtime starts a worker per CPU), so other machines get budget entries of their own.

The numbers live in [`scripts/efficiency/budget.json`](../scripts/efficiency/budget.json), with one entry per machine: `windows` (the table above), `windows-ci` (GitHub's x64 runner, at rest, no Flipper: about twice its measured numbers) and `linux` (report only, until it has a baseline). Change a budget only in a pull request of its own that says why.

## How each number is measured

[`scripts/efficiency/measure.py`](../scripts/efficiency/measure.py) (Python 3 and `psutil`) starts a build, or watches the running app, and measures it for 60 seconds:

- **Private memory:** the peak of the memory only this app uses (private bytes on Windows, USS on Linux).
- **CPU:** CPU time over wall time, as a share of one core. Task Manager shows a share of the whole machine instead: on 12 CPUs, 1.2 % of one core shows as 0.1 %.
- **Wake-ups:** context switches per second of all the app's threads. Windows: the `Thread` performance counters (their names are English only). Linux: `/proc/<pid>/task/*/status`.
- **Battery:** by hand. On a laptop on battery, after an hour of normal use, Task Manager's **Processes** tab, column **Power usage**. No script measures it reliably.

The script asks the app over its [control socket](../desktop/app/README.md#for-coding-agents) whether it runs and how long you've been idle. If the app doesn't answer, didn't see the synthetic input, or saw someone use the computer during an at-rest window, the result is "couldn't measure" (exit code 2), never a pass.

```bash
python -m pip install psutil
python scripts/efficiency/measure.py --exe desktop/app/target/release/duckwalk.exe   # at rest
python scripts/efficiency/measure.py --pid <pid> --working --report working.md       # the running app, while you work
```

With `--exe` it refuses to start while another Duckwalk runs: quit that one from Harold's menu, or measure it with `--pid`.

## When it runs

- **Every pull request:** the **Desktop app** workflow measures the Windows build at rest against `windows-ci`, and the Linux build under a virtual screen for the report only. The table is in the run summary.
- **Before every release:** step 4 of [Releasing](development.md#releasing).

## Baselines

| Date | Machine | State | Private memory | CPU, % of one core | Wake-ups/s |
|---|---|---|---|---|---|
| 6 Nov 2026 | Windows 11 ARM64 | at rest | about 9 MB | 0.1 % in Task Manager | – |
| 25 Nov 2026 | Windows 11 ARM64, Bluetooth | at rest | 9.4–10.4 MB | 1.3–1.5 % | 207–218 |
| 25 Nov 2026 | Windows 11 ARM64, Bluetooth | while you work | 9.4 MB | 1.8–2.1 % | 335–383 |
| 27 Nov 2026 | Windows 11 ARM64, Bluetooth, #201 | at rest | 9.2 MB | 0.52–0.55 % | 136–176 |
| 27 Nov 2026 | Windows 11 ARM64, Bluetooth, #201 | while you work | 9.1–9.6 MB | 0.83–0.96 % | 371–401 |
| 25 Nov 2026 | GitHub `windows-latest`, x64, no Flipper | at rest | 7.6–8.9 MB | 0.29–0.47 % | 131–152 |
| 25 Nov 2026 | GitHub `ubuntu-latest`, xvfb, no Flipper | at rest, input not watched | 68.7 MB (USS, most of it GTK) | 0.20 % | 71 |

Where the wake-ups come from today: the tray redraws every 250 to 500 ms, and your own input wakes the thread that watches it. The main loop reads Duckwalk's packets every 50 ms only while it is open over USB, and sends the pointer's area every 100 ms only while Harold walks. The Bluetooth link sleeps until there is a new packet or a heartbeat is up. Before #201 all three checked all the time: on 27 Nov the build before it measured 297–425 wake-ups a second at rest and 549–551 while you work.

---

[Docs home](README.md)
