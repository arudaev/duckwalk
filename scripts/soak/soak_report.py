"""Summarise a soak run from the logs of soak_pc and soak_flipper.

    python soak_report.py pc.csv [flipper.csv] [--from "2026-11-04 08:00"] [--to ...]

Prints what the run shows against the soak's acceptance criteria (#65): the
Windows idle time never reaches the limit, the PC never locks, the Flipper's
free heap does not shrink and Duckwalk is still running. Markdown output, so it
can be pasted into the pull request.
"""
import argparse
import re
from datetime import datetime

FMT = "%Y-%m-%d %H:%M:%S"


def parse_time(s):
    for fmt in (FMT, "%Y-%m-%d %H:%M", "%Y-%m-%d"):
        try:
            return datetime.strptime(s, fmt)
        except ValueError:
            pass
    raise SystemExit("bad time: %s" % s)


def read_rows(path, start, stop):
    """Yield (time, {key: value}) for data lines, and collect device names."""
    rows, names = [], {}
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.strip()
        if not line:
            continue
        if line.startswith("#"):
            m = re.match(r"#\s+(?:\S+ \S+ )?(dev\d+) (.+)", line)
            if m:
                names[m.group(1)] = m.group(2)
            continue
        stamp, _, rest = line.partition(",")
        try:
            t = datetime.strptime(stamp, FMT)
        except ValueError:
            continue
        if (start and t < start) or (stop and t > stop):
            continue
        fields = dict(kv.split("=", 1) for kv in rest.split(",") if "=" in kv)
        rows.append((t, fields))
    return rows, names


def fmt_span(seconds):
    h, m = divmod(int(seconds // 60), 60)
    return "%d h %02d min" % (h, m)


def label(name):
    n = name.lower()
    if "vid_1209" in n:
        return "Duckwalk over USB"
    if n in ("no-device", "injected", "?"):
        return "touchpad or software"
    if re.search(r"_80e12[67]", n):
        return "Flipper over Bluetooth"  # STMicroelectronics address prefix
    if "{00001812" in n or "{00001124" in n:
        return "Bluetooth mouse"  # HID over GATT (LE) or classic HID
    return "mouse"


def pc_report(rows, names, idle_limit):
    out, ok = [], True
    t0, t1 = rows[0][0], rows[-1][0]
    out.append("**PC log** %s to %s (%s, %d minutes logged)" % (t0.strftime(FMT), t1.strftime(FMT),
                                                                  fmt_span((t1 - t0).total_seconds()), len(rows)))
    gaps = [(a, b) for (a, _), (b, _) in zip(rows, rows[1:]) if (b - a).total_seconds() > 150]
    idle = [(t, int(f.get("idle_s", 0))) for t, f in rows]
    worst_t, worst = max(idle, key=lambda x: x[1])
    over = [t for t, s in idle if s >= idle_limit]
    locked = [t for t, f in rows if f.get("locked") == "1"]

    out.append("")
    out.append("- Longest Windows idle time: %d s (at %s); limit %d s" % (worst, worst_t.strftime("%H:%M"), idle_limit))
    if over:
        ok = False
        out.append("- **Idle reached the limit in %d minutes**, first at %s" % (len(over), over[0].strftime(FMT)))
    if locked:
        ok = False
        out.append("- **Locked for %d minutes**, first at %s" % (len(locked), locked[0].strftime(FMT)))
    for a, b in gaps:
        out.append("- No log from %s to %s (PC asleep or logger stopped)" % (a.strftime(FMT), b.strftime(FMT)))

    devices = sorted({k for _, f in rows for k in f if k.startswith("dev")}, key=lambda d: int(d[3:]))
    if devices:
        out.append("")
        out.append("| Device | What | Minutes with reports | Reports |")
        out.append("|---|---|---|---|")
        # A device that reconnects gets a new handle; group the handles by name.
        groups = {}
        for d in devices:
            groups.setdefault(names.get(d, "?"), []).append(d)
        for name, ds in groups.items():
            per_min = [sum(int(f[d].split("/")[0]) for d in ds if d in f) for _, f in rows]
            active = sum(1 for c in per_min if c)
            if active:
                ids = ds[0] if len(ds) == 1 else "%s (+%d reconnects)" % (ds[0], len(ds) - 1)
                out.append("| %s | %s | %d | %d |" % (ids, label(name), active, sum(per_min)))
    return out, ok


def flipper_report(rows):
    out, ok = [], True
    read = [(t, f) for t, f in rows if "heap_free" in f and f["heap_free"].isdigit()]
    absent = [t for t, f in rows if f.get("cli") in ("absent", "busy") or "error" in f]
    out.append("**Flipper log** %d readings, %d with the command line away (USB mode, unplugged or busy)" % (
        len(rows), len(absent)))
    if not read:
        out.append("")
        out.append("- No heap readings. In USB mode the app owns the port; read the heap after the run instead.")
        return out, ok

    out.append("")
    first, last = read[0], read[-1]
    heaps = [int(f["heap_free"]) for _, f in read]
    out.append("- Free heap: %d at the start, %d at the end, lowest %d" % (heaps[0], heaps[-1], min(heaps)))
    floor = [int(f["heap_min"]) for _, f in read if f.get("heap_min", "").isdigit()]
    if floor:
        out.append("- Lowest heap ever (firmware's watermark): %d at the start, %d at the end" % (floor[0], floor[-1]))
    if heaps[-1] < heaps[0] - 4096:
        ok = False
        out.append("- **Free heap shrank by %d bytes**" % (heaps[0] - heaps[-1]))

    not_running = [t for t, f in read if f.get("app") != "duckwalk"]
    if not_running:
        ok = False
        out.append("- **Duckwalk was not running at %d readings**, first at %s" % (len(not_running),
                                                                                 not_running[0].strftime(FMT)))
    else:
        out.append("- Duckwalk was running at every reading, the last at %s" % last[0].strftime(FMT))

    # Battery: every stretch where it was discharging, or unplugged between two readings.
    charged = [(t, int(f["charge"]), f.get("state", "?")) for t, f in read if f.get("charge", "").isdigit()]
    for (ta, ca, sa), (tb, cb, sb) in zip(charged, charged[1:]):
        hours = (tb - ta).total_seconds() / 3600
        if cb < ca and hours > 0.5:
            out.append("- Battery %d%% to %d%% from %s to %s: %.1f %%/h" % (
                ca, cb, ta.strftime("%H:%M"), tb.strftime("%H:%M"), (ca - cb) / hours))
    return out, ok


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("pc")
    ap.add_argument("flipper", nargs="?")
    ap.add_argument("--from", dest="start", type=parse_time)
    ap.add_argument("--to", dest="stop", type=parse_time)
    ap.add_argument("--idle-limit", type=int, default=120, help="seconds (default 120)")
    args = ap.parse_args()

    rows, names = read_rows(args.pc, args.start, args.stop)
    if not rows:
        raise SystemExit("no PC readings in that range")
    lines, ok = pc_report(rows, names, args.idle_limit)
    if args.flipper:
        frows, _ = read_rows(args.flipper, args.start, args.stop)
        more, fok = flipper_report(frows) if frows else (["**Flipper log** no readings in that range"], True)
        lines += [""] + more
        ok = ok and fok
    print("\n".join(lines))
    print("")
    print("**Result: %s**" % ("passed" if ok else "needs a look"))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
