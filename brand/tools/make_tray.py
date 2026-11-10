"""Draw Harold for the tray: the frames the desktop app's tray icon shows.

Same master drawings as the Flipper sprites and the app icon
(brand/source/harold-24*.txt), in Harold's colours (make_icon.py), one cell
per pixel, centred on 32x32.

    pip install pillow
    python brand/tools/make_tray.py [--preview DIR]

Frames, left to right in desktop/app/assets/harold-tray.png:
    idle       standing: awake, or waiting while you work
    waddle1-4  walking: lean back, hop, lean forward, hop
    asleep     eyes shut: keep-awake off
    quack      bill open, for a moment
    grey       standing, grey: no Flipper connected
"""

import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).parent))
from make_icon import BEAK, CLEAR, DUCK, GLINT, INK, colours  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "brand" / "source"
OUT = ROOT / "desktop" / "app" / "assets" / "harold-tray.png"

SIZE = 32
OX, OY = 4, 4  # where the 24x24 drawing sits

NAMES = ["idle", "waddle1", "waddle2", "waddle3", "waddle4", "asleep", "quack", "grey"]

# The open bill in harold-24-quack.txt: the upper half (rows 6-7) and the
# lower half (rows 9-10), right of the head, as (row, column).
QUACK_BILL = {(6, 19), (6, 20), (6, 21), (6, 22), (7, 19), (7, 20), (7, 21), (9, 19), (9, 20), (10, 20), (10, 21)}


def load(name: str) -> list[list[bool]]:
    rows = [line.strip() for line in (SRC / name).read_text().splitlines() if line.strip()]
    return [[c == "#" for c in row] for row in rows]


def quack_colours(quack_ink, idle_pixels):
    """The open bill leaves a gap in the outline, so a flood fill would leak
    out of it: the quack takes its fill from the standing drawing instead,
    yellow for the body, orange for the bill, nothing in the open mouth."""
    out = []
    for y, row in enumerate(quack_ink):
        line = []
        for x, ink in enumerate(row):
            if ink:
                line.append(INK)
            elif (y, x) in QUACK_BILL:
                line.append(BEAK)
            elif idle_pixels[y][x] == GLINT:
                line.append(GLINT)
            elif idle_pixels[y][x] in (DUCK, BEAK) and x < 20:
                line.append(DUCK)
            else:
                line.append(CLEAR)
        out.append(line)
    # A cell left empty but walled in on all four sides is body.
    for y in range(1, len(out) - 1):
        for x in range(1, len(out[0]) - 1):
            if out[y][x] == CLEAR and all(out[y + dy][x + dx] != CLEAR for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                out[y][x] = DUCK
    return out


def grey(c):
    level = (c[0] * 3 + c[1] * 6 + c[2]) // 10
    return (level, level, level, 140)


def frame(pixels, dy=0, lean=0, tint=None) -> Image.Image:
    """Harold on a 32x32 canvas. `lean` shifts his upper half one pixel back
    (-1) or forward (+1), the way a waddling duck rocks; `dy` lifts him."""
    img = Image.new("RGBA", (SIZE, SIZE), CLEAR)
    h = len(pixels)
    for y, row in enumerate(pixels):
        shift = lean if y < h // 2 else 0
        for x, c in enumerate(row):
            if c[3]:
                img.putpixel((OX + x + shift, OY + y + dy), tint(c) if tint else c)
    return img


def frames() -> list[Image.Image]:
    idle = colours(load("harold-24.txt"))
    asleep = colours(load("harold-24-blink.txt"))
    quack = quack_colours(load("harold-24-quack.txt"), idle)
    return [
        frame(idle),
        frame(idle, lean=-1),
        frame(idle, dy=-1),
        frame(idle, lean=1),
        frame(idle, dy=-1),
        frame(asleep),
        frame(quack),
        frame(idle, tint=grey),
    ]


def main() -> None:
    fs = frames()
    strip = Image.new("RGBA", (SIZE * len(fs), SIZE), CLEAR)
    for i, f in enumerate(fs):
        strip.paste(f, (i * SIZE, 0))
    strip.save(OUT)
    print(f"wrote {OUT.relative_to(ROOT)}: {' '.join(NAMES)}")

    if "--preview" in sys.argv:
        out = Path(sys.argv[sys.argv.index("--preview") + 1])
        out.mkdir(parents=True, exist_ok=True)
        preview(fs, out)


def preview(fs, out: Path) -> None:
    """Every frame at 1x, 2x and 4x, on a light and a dark taskbar, and the
    waddle as a GIF."""
    pad, scales = 8, (1, 2, 4)
    w = len(fs) * (SIZE * 4 + pad) + pad
    h = sum(SIZE * s + pad for s in scales) + pad
    sheet = Image.new("RGBA", (w, h * 2), (243, 243, 243, 255))
    sheet.paste((32, 32, 32, 255), (0, h, w, h * 2))
    for half in (0, h):
        y = half + pad
        for s in scales:
            for i, f in enumerate(fs):
                big = f.resize((SIZE * s, SIZE * s), Image.NEAREST)
                sheet.paste(big, (pad + i * (SIZE * 4 + pad), y), big)
            y += SIZE * s + pad
    sheet.save(out / "harold-tray-sheet.png")

    walk = [fs[1], fs[2], fs[3], fs[4]] * 3 + [fs[0]] * 4 + [fs[6]] * 2 + [fs[0]] * 4
    gif = []
    for f in walk:
        bg = Image.new("RGBA", (SIZE * 4, SIZE * 4), (32, 32, 32, 255))
        big = f.resize((SIZE * 4, SIZE * 4), Image.NEAREST)
        bg.paste(big, (0, 0), big)
        gif.append(bg.convert("P", palette=Image.ADAPTIVE))
    gif[0].save(out / "harold-tray-walk.gif", save_all=True, append_images=gif[1:], duration=250, loop=0)
    print(f"wrote previews to {out}")


if __name__ == "__main__":
    main()
