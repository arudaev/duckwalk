"""Soak logger for the Flipper side.

Every few minutes, read the Flipper's free heap, battery and running app over
its USB command line, and append one CSV line.

The command line is only there while the Flipper's own USB device is active:
in Duckwalk's USB mode the app owns the port, and unplugged there is no cable.
Those readings are logged as cli=absent and the logger carries on, so a
Bluetooth soak can be unplugged for the battery test and plugged back in.

Needs pyserial (python -m pip install pyserial). Close qFlipper first.

    python soak_flipper.py flipper.csv 24            # file, hours
    python soak_flipper.py flipper.csv 24 --port COM3 --every 300
"""
import argparse
import re
import time

import serial
from serial.tools import list_ports

FLIPPER_VID, FLIPPER_PID = 0x0483, 0x5740


def find_port():
    for p in list_ports.comports():
        if p.vid == FLIPPER_VID and p.pid == FLIPPER_PID:
            return p.device
    return None


def query(port):
    s = serial.Serial(port, 230400, timeout=0.3)
    try:
        time.sleep(0.3)
        s.read(10000)
        text = ""
        for c in ("free", "info power", "loader info"):
            s.write((c + "\r").encode())
            time.sleep(1.0)
            text += s.read(20000).decode(errors="replace")
    finally:
        s.close()
    text = re.sub(r"\x1b\[[0-9;]*m", "", text)

    def grab(pat):
        m = re.search(pat, text)
        return m.group(1) if m else "?"

    if "Duckwalk" in text and "is running" in text:
        app = "duckwalk"
    elif "No application" in text:
        app = "none"
    else:
        app = "?"
    return "heap_free=%s,heap_min=%s,charge=%s,state=%s,app=%s" % (
        grab(r"Free heap size: (\d+)"),
        grab(r"Minimum heap size: (\d+)"),
        grab(r"charge\.level\s*:\s*(\d+)"),
        grab(r"charge\.state\s*:\s*(\w+)"),
        app,
    )


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("out")
    ap.add_argument("hours", type=float, nargs="?", default=24)
    ap.add_argument("--port", help="COM port; found by USB ID if left out")
    ap.add_argument("--every", type=int, default=300, help="seconds between readings")
    args = ap.parse_args()

    end = time.time() + args.hours * 3600
    print("Logging to %s for %.1f h. Ctrl+C stops it." % (args.out, args.hours))
    while time.time() < end:
        stamp = time.strftime("%Y-%m-%d %H:%M:%S")
        port = args.port or find_port()
        if port is None:
            line = "cli=absent"
        else:
            try:
                line = query(port)
            except serial.SerialException:
                line = "cli=busy"  # qFlipper has the port, or it vanished mid-read
        with open(args.out, "a") as f:
            f.write("%s,%s\n" % (stamp, line))
        time.sleep(args.every)


if __name__ == "__main__":
    main()
