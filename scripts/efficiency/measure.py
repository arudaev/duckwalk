"""Check the desktop app against its efficiency budget (docs/efficiency.md).

Starts the app (or watches one that runs), lets it settle, then measures
private memory, CPU and wake-ups over a window and compares them with an
entry of budget.json. Exit 0: within budget; 1: over; 2: couldn't measure.

    python scripts/efficiency/measure.py --exe desktop/app/target/release/duckwalk.exe
    python scripts/efficiency/measure.py --pid 1234 --working --report working.md

It asks the app itself, over its control socket, whether it runs and how
long you've been idle. A copy that never really started, or a window in
which someone used the computer, is "couldn't measure", never a pass.
"""

import argparse
import json
import pathlib
import platform
import socket
import subprocess
import sys
import threading
import time

import psutil

HERE = pathlib.Path(__file__).resolve().parent
WINDOWS = sys.platform == "win32"
LINUX = sys.platform.startswith("linux")
PIPE = r"\\.\pipe\duckwalk-control"
STATUS = b'{"cmd": "status"}\n'


def ask_app():
    """The app's status from its control socket; None if it doesn't answer."""
    try:
        if WINDOWS:
            with open(PIPE, "r+b", buffering=0) as f:
                f.write(STATUS)
                line = f.readline()
        else:
            with socket.socket(socket.AF_UNIX) as s:
                s.settimeout(5)
                s.connect("\0duckwalk-control")
                s.sendall(STATUS)
                line = s.makefile("rb").readline()
        return json.loads(line)
    except (OSError, ValueError):
        return None


def tray_processes():
    """Running tray apps, leaving out `duckwalk mcp`."""
    found = []
    for p in psutil.process_iter(["name", "cmdline"]):
        name = (p.info["name"] or "").lower()
        if name in ("duckwalk", "duckwalk.exe") and "mcp" not in (p.info["cmdline"] or [])[1:]:
            found.append(p)
    return found


def private_mb(p):
    """Memory only this process uses: private bytes on Windows, USS on Linux."""
    if WINDOWS:
        return p.memory_info().private / 2**20
    return p.memory_full_info().uss / 2**20


def cpu_seconds(p):
    t = p.cpu_times()
    return t.user + t.system


def linux_switches(pid):
    """Context switches of every thread so far, from /proc."""
    total = 0
    for task in pathlib.Path(f"/proc/{pid}/task").iterdir():
        try:
            for line in (task / "status").read_text().splitlines():
                if line.startswith(("voluntary_ctxt_switches", "nonvoluntary_ctxt_switches")):
                    total += int(line.split()[1])
        except OSError:
            pass  # the thread ended while we read
    return total


# One Get-Counter call for the whole window: both counters in the same call,
# so each thread's rate can be matched to its process.
WINDOWS_COUNTERS = r"""
$s = Get-Counter -Counter '\Thread({name}*)\Context Switches/sec','\Thread({name}*)\ID Process' -SampleInterval 1 -MaxSamples {n} -ErrorAction SilentlyContinue
foreach ($set in $s) {{
  $ids = @{{}}; $rates = @{{}}
  foreach ($c in $set.CounterSamples) {{
    if ($c.Status -ne 0) {{ continue }}  # a thread that came or went this second
    if ($c.Path -like '*\id process') {{ $ids[$c.InstanceName] = $c.RawValue }}
    else {{ $rates[$c.InstanceName] = $c.CookedValue }}
  }}
  $sum = 0
  foreach ($k in $rates.Keys) {{ if ($ids[$k] -eq {pid}) {{ $sum += $rates[$k] }} }}
  $sum
}}
"""


def windows_wakeups(name, pid, seconds, out):
    """Average context switches per second of the app's threads."""
    script = WINDOWS_COUNTERS.format(name=name, pid=pid, n=max(2, int(seconds)))
    r = subprocess.run(["powershell", "-NoProfile", "-Command", script],
                       capture_output=True, text=True)
    values = [float(v.replace(",", ".")) for v in r.stdout.split()]
    out["wakeups"] = sum(values) / len(values) if values else None
    if not values:
        out["error"] = ("no Thread counters (their names are English only, so on Windows "
                        "in another language read them in Performance Monitor). "
                        + r.stderr.strip()[:200])


def nudge_while(stop):
    """Synthetic input: the pointer one pixel right, then back, ten moves a
    second. (Windows drops a move of 0 px, so the app wouldn't see it.)"""
    import ctypes

    class MOUSEINPUT(ctypes.Structure):
        _fields_ = [("dx", ctypes.c_long), ("dy", ctypes.c_long),
                    ("mouseData", ctypes.c_ulong), ("dwFlags", ctypes.c_ulong),
                    ("time", ctypes.c_ulong), ("dwExtraInfo", ctypes.c_size_t)]

    class INPUT(ctypes.Structure):  # MOUSEINPUT is the union's largest member
        _fields_ = [("type", ctypes.c_ulong), ("mi", MOUSEINPUT)]

    send = ctypes.windll.user32.SendInput
    step = 1
    while not stop.is_set():
        move = INPUT(0, MOUSEINPUT(step, 0, 0, 0x0001, 0, 0))  # INPUT_MOUSE, MOUSEEVENTF_MOVE
        if send(1, ctypes.byref(move), ctypes.sizeof(INPUT)) != 1:
            return  # blocked (not the input desktop); measure() reports it
        step = -step
        stop.wait(0.1)


def measure(p, seconds, working, report_only=False):
    stop = threading.Event()
    if working:
        if not WINDOWS:
            raise RuntimeError("--working makes input on Windows only")
        threading.Thread(target=nudge_while, args=(stop,), daemon=True).start()
        time.sleep(2)

    out = {}
    counters = None
    if WINDOWS:
        counters = threading.Thread(target=windows_wakeups,
                                    args=(pathlib.Path(p.exe()).stem, p.pid, seconds, out))
        counters.start()
    switches0 = linux_switches(p.pid) if LINUX else 0
    cpu0, t0 = cpu_seconds(p), time.monotonic()
    peak = private_mb(p)
    idle = []
    try:
        while time.monotonic() - t0 < seconds:
            time.sleep(1)
            if not p.is_running():
                raise RuntimeError("the app stopped during the measurement")
            peak = max(peak, private_mb(p))
            idle.append((ask_app() or {}).get("your_idle_seconds"))
        dt = time.monotonic() - t0
        out["cpu"] = (cpu_seconds(p) - cpu0) / dt * 100
        if LINUX:
            out["wakeups"] = (linux_switches(p.pid) - switches0) / dt
    finally:
        stop.set()
        if counters:
            counters.join()
    out["private"] = peak
    out["working"] = working

    known = [i for i in idle if i is not None]
    if not known:
        if report_only:
            out["note"] = "The app couldn't watch input here, so nobody checked that the computer was left alone."
            return out
        raise RuntimeError("the app didn't say how long you've been idle "
                           "(no control socket, or it can't watch input)")
    if working and max(known) > 1:
        raise RuntimeError(f"the app didn't see the synthetic input (idle up to {max(known)} s); "
                           "run it on the desktop you're signed in to")
    if not working and any(b < a for a, b in zip(known, known[1:])):
        raise RuntimeError("someone used the computer during the measurement; "
                           "measure again without touching it")
    return out


def report(m, budget, name, seconds):
    state = "while you work (synthetic input, 10 a second)" if m["working"] else "at rest"
    rows = [
        ("Private memory (peak)", m["private"], budget.get("private_mb"), "{:.1f} MB"),
        ("CPU, % of one core", m["cpu"],
         budget.get("cpu_working_pct" if m["working"] else "cpu_rest_pct"), "{:.2f} %"),
        ("Wake-ups per second", m.get("wakeups"),
         budget.get("wakeups_working" if m["working"] else "wakeups_rest"), "{:.0f}"),
    ]
    lines = [
        f"### Desktop app efficiency, {state}",
        "",
        f"{platform.system()} {platform.release()}, {platform.machine()}, "
        f"{psutil.cpu_count()} logical CPUs, {seconds:.0f} s. "
        f"Budget `{name}`: {budget.get('about', '')}.",
        "",
        "| Metric | Measured | Budget | |",
        "|---|---|---|---|",
    ]
    over = missing = False
    for label, value, limit, fmt in rows:
        shown_limit = fmt.format(limit) if limit is not None else "–"
        if value is None:
            missing = True
            lines.append(f"| {label} | not measured | {shown_limit} | ? |")
        elif limit is None:
            lines.append(f"| {label} | {fmt.format(value)} | – | report only |")
        else:
            ok = value <= limit
            over |= not ok
            lines.append(f"| {label} | {fmt.format(value)} | {shown_limit} | "
                         f"{'ok' if ok else 'over'} |")
    if m.get("error"):
        lines += ["", f"Wake-ups: {m['error']}"]
    if m.get("note"):
        lines += ["", m["note"]]
    return "\n".join(lines) + "\n", (1 if over else 2 if missing else 0)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    who = ap.add_mutually_exclusive_group(required=True)
    who.add_argument("--exe", help="start this build with --no-install")
    who.add_argument("--pid", type=int, help="measure the app that runs as this process")
    ap.add_argument("--seconds", type=float, default=60)
    ap.add_argument("--warmup", type=float, default=15,
                    help="seconds to let a started app settle first")
    ap.add_argument("--working", action="store_true",
                    help="Windows: synthetic input ten times a second")
    ap.add_argument("--budget", default="windows" if WINDOWS else "linux",
                    help="which entry of budget.json to check against")
    ap.add_argument("--report", help="also write the Markdown table to this file")
    args = ap.parse_args()

    budgets = json.loads((HERE / "budget.json").read_text(encoding="utf-8"))
    if args.budget not in budgets:
        print(f"No budget called {args.budget!r}; budget.json has {', '.join(budgets)}.")
        return 2
    budget = budgets[args.budget]
    if args.exe and tray_processes():
        print("Duckwalk is already running. Quit it from Harold's menu, or measure it with --pid.")
        return 2

    started = None
    try:
        if args.exe:
            started = subprocess.Popen([args.exe, "--no-install"],
                                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(args.warmup)
            if started.poll() is not None:
                print(f"The app exited with {started.returncode} before the measurement.")
                return 2
            p = psutil.Process(started.pid)
        else:
            p = psutil.Process(args.pid)
        if ask_app() is None:
            print("The app doesn't answer on its control socket, so it isn't really running.")
            return 2
        report_only = not any(isinstance(v, (int, float)) for v in budget.values())
        m = measure(p, args.seconds, args.working, report_only)
    except (psutil.Error, RuntimeError, OSError) as e:
        print(f"Couldn't measure: {e}")
        return 2
    finally:
        if started and started.poll() is None:
            started.kill()
            started.wait(10)

    text, code = report(m, budget, args.budget, args.seconds)
    print(text)
    if args.report:
        pathlib.Path(args.report).write_text(text, encoding="utf-8")
    return code


if __name__ == "__main__":
    sys.exit(main())
