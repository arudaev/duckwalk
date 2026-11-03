"""Draw Duckwalk's pixel sprites and app icon from Harold's master drawing.

Harold is a rubber duck, the one from VOYAGER // 56K. His drawings are text
grids in brand/source, '#' for ink and '.' for paper, 24x24, facing right:
harold-24.txt (idle), harold-24-blink.txt and harold-24-quack.txt. Edit a
pixel there and run this to redraw every sprite:

    python brand/tools/make_brand.py

Writes:
    app/images/harold_*.png   1-bit sprites for the Flipper (black = pixel on)
    app/icon.png              10x10 1-bit app icon (also images/harold_mini)
    brand/sprites-preview.png the sprites enlarged, for reviewing art changes

Poses, each 24x25 (one row of headroom for the bob), and a mirrored "_l"
copy facing left:
    idle   harold-24.txt
    bob    idle one pixel higher: a walking or typing bounce
    blink  harold-24-blink.txt, eyes closed; also used while asleep
    quack  harold-24-quack.txt, the bill open

Each pose also gets a mask ("_mask"): Harold filled in solid, so the app can
blank out what he stands in front of before drawing him.

The README banner (brand/banner.png) is a separate pen drawing, not made here.
"""

from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "brand" / "source"

INK = (26, 26, 26)
LCD = (255, 130, 0)  # the Flipper's orange, the one flat colour field


def load(name):
    path = SOURCE / name
    rows = [line.strip() for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
    if len({len(r) for r in rows}) != 1:
        raise SystemExit(f"{path}: every row must be the same width")
    return [[1 if c == "#" else 0 for c in row] for row in rows]


def headroom(bitmap, raised=False):
    """One empty row: on top normally, at the bottom when bobbing up."""
    empty = [0] * len(bitmap[0])
    return bitmap + [empty] if raised else [empty] + bitmap


def mirror(bitmap):
    return [list(reversed(row)) for row in bitmap]


def silhouette(bitmap):
    """The sprite filled in: every pixel not reachable from the edge."""
    h, w = len(bitmap), len(bitmap[0])
    outside = [[False] * w for _ in range(h)]
    todo = [(x, y) for x in range(w) for y in (0, h - 1)] + [(x, y) for y in range(h) for x in (0, w - 1)]
    while todo:
        x, y = todo.pop()
        if not (0 <= x < w and 0 <= y < h) or outside[y][x] or bitmap[y][x]:
            continue
        outside[y][x] = True
        todo += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    return [[0 if outside[y][x] else 1 for x in range(w)] for y in range(h)]


def to_image(bitmap, on=(0, 0, 0), off=(255, 255, 255), mode="1"):
    h, w = len(bitmap), len(bitmap[0])
    im = Image.new("RGB", (w, h), off)
    for y in range(h):
        for x in range(w):
            if bitmap[y][x]:
                im.putpixel((x, y), on)
    return im.convert("1") if mode == "1" else im


def icon_bitmap():
    """10x10 Harold for the app list and Preview, facing right like the sprites."""
    rows = [
        ".....XXX..",
        "....X...X.",
        "....X.X.XX",
        ".X..X...XX",
        ".XX.X..X..",
        ".X.X....X.",
        ".X.......X",
        ".X.......X",
        "..X.....X.",
        "...XXXXX..",
    ]
    return [[1 if c == "X" else 0 for c in row] for row in rows]


def draw_pixels(draw, bitmap, x0, y0, size, colour):
    for y, row in enumerate(bitmap):
        for x, v in enumerate(row):
            if v:
                draw.rectangle([x0 + x * size, y0 + y * size, x0 + (x + 1) * size - 1, y0 + (y + 1) * size - 1], fill=colour)


def main():
    idle = load("harold-24.txt")
    poses = {
        "idle": headroom(idle),
        "bob": headroom(idle, raised=True),
        "blink": headroom(load("harold-24-blink.txt")),
        "quack": headroom(load("harold-24-quack.txt")),
    }
    # The open bill lets a fill leak inside, so every mask is filled in
    # together with the idle body at the same height.
    body = {"idle": poses["idle"], "bob": poses["bob"], "blink": poses["idle"], "quack": poses["idle"]}
    sprites, masks = {}, {}
    for name, bm in poses.items():
        filled = silhouette(bm)
        base = silhouette(body[name])
        mask = [[a | b for a, b in zip(r1, r2)] for r1, r2 in zip(filled, base)]
        sprites[f"harold_{name}"] = bm
        sprites[f"harold_{name}_l"] = mirror(bm)
        masks[f"harold_{name}"] = mask
        masks[f"harold_{name}_l"] = mirror(mask)

    images = ROOT / "app" / "images"
    images.mkdir(parents=True, exist_ok=True)
    for old in images.glob("harold_*.png"):
        old.unlink()
    for name, bm in sprites.items():
        to_image(bm).save(images / f"{name}_{len(bm[0])}x{len(bm)}.png")
        mask = masks[name]
        to_image(mask).save(images / f"{name}_mask_{len(bm[0])}x{len(bm)}.png")
    to_image(icon_bitmap()).save(ROOT / "app" / "icon.png")
    # The icon doubles as the small Harold that traces a walk in Preview.
    to_image(icon_bitmap()).save(images / "harold_mini_10x10.png")

    # Preview sheet: every sprite at 8x on the Flipper orange.
    pad, z = 16, 8
    widths = [len(bm[0]) * z for bm in sprites.values()]
    height = max(len(bm) for bm in sprites.values()) * z
    sheet = Image.new("RGB", (sum(widths) + pad * (len(widths) + 1), height + 2 * pad), LCD)
    x = pad
    sd = ImageDraw.Draw(sheet)
    for bm in sprites.values():
        draw_pixels(sd, bm, x, pad, z, INK)
        x += len(bm[0]) * z + pad
    sheet.save(ROOT / "brand" / "sprites-preview.png")

    print("sprites:", ", ".join(sprites), "| icon and preview written")


if __name__ == "__main__":
    main()
