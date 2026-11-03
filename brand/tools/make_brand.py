"""Draw Duckwalk's pixel sprites and app icon from Harold's 16 px source sprite.

Harold comes from VOYAGER // 56K (brand/source/harold-16.png). Everything
here is derived from that silhouette so the duck looks the same everywhere.

    python brand/tools/make_brand.py

Writes:
    app/images/harold_*.png   1-bit sprites for the Flipper (black = pixel on)
    app/icon.png              10x10 1-bit app icon (also images/harold_mini)
    brand/sprites-preview.png the sprites enlarged, for reviewing art changes

The README banner (brand/banner.png) is a separate pen drawing, not made here.
"""

from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "brand" / "source" / "harold-16.png"

INK = (26, 26, 26)
LCD = (255, 130, 0)  # the Flipper's orange, the one flat colour field


def load_source():
    """Return the 16x16 grid: 'Y' body, 'K' beak, 'E' eye, '.' empty."""
    im = Image.open(SOURCE).convert("RGBA")
    grid = []
    for y in range(im.height):
        row = ""
        for x in range(im.width):
            r, g, b, a = im.getpixel((x, y))
            if a < 128:
                row += "."
            elif r > 200 and g > 150:
                row += "Y"
            elif r > 150:
                row += "K"
            else:
                row += "E"
        grid.append(row)
    return grid


def crop(grid):
    ys = [y for y, row in enumerate(grid) if row.strip(".")]
    xs = [x for row in grid for x, c in enumerate(row) if c != "."]
    return [row[min(xs):max(xs) + 1] for row in grid[min(ys):max(ys) + 1]]


def scale(grid, n):
    return ["".join(c * n for c in row) for row in grid for _ in range(n)]


def outline(grid, eye_closed=False):
    """1-bit drawing: body as an ink outline, beak and eye solid."""
    h, w = len(grid), len(grid[0])
    inside = lambda x, y: 0 <= x < w and 0 <= y < h and grid[y][x] != "."
    out = [[0] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            c = grid[y][x]
            if c == "K":
                out[y][x] = 1
            elif c == "E":
                out[y][x] = 0 if eye_closed else 1
            elif c == "Y":
                edge = any(not inside(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
                out[y][x] = 1 if edge else 0
    if eye_closed:
        eyes = [(x, y) for y in range(h) for x in range(w) if grid[y][x] == "E"]
        ey = max(y for _, y in eyes)
        for x in {x for x, _ in eyes} | {min(x for x, _ in eyes) - 1}:
            out[ey][x] = 1
    return out


def open_beak(grid):
    """Quack: the top half of the beak moves up a row, the bottom half down."""
    beak = [(x, y) for y, row in enumerate(grid) for x, c in enumerate(row) if c == "K"]
    mid = (min(y for _, y in beak) + max(y for _, y in beak) + 1) / 2
    rows = [list(row.replace("K", ".")) for row in grid]
    for x, y in beak:
        rows[y - 1 if y < mid else y + 1][x] = "K"
    return ["".join(row) for row in rows]


def with_feet(bitmap, frame):
    """Add a row of feet under the body; two frames alternate the stride."""
    w = len(bitmap[0])
    feet = [0] * w
    left, right = (10, 18) if frame == 0 else (12, 16)
    for x in (left, left + 1, left + 2, right, right + 1, right + 2):
        if x < w:
            feet[x] = 1
    return bitmap + [feet, [0] * w]


def bob(bitmap):
    """Raise the body one pixel (walking bounce)."""
    return bitmap[1:] + [[0] * len(bitmap[0])]


def mirror(bitmap):
    return [list(reversed(row)) for row in bitmap]


def to_image(bitmap, on=(0, 0, 0), off=(255, 255, 255), mode="1"):
    h, w = len(bitmap), len(bitmap[0])
    im = Image.new("RGB", (w, h), off)
    for y in range(h):
        for x in range(w):
            if bitmap[y][x]:
                im.putpixel((x, y), on)
    return im.convert("1") if mode == "1" else im


def icon_bitmap():
    """10x10 Harold, fitted by hand from the 16 px silhouette."""
    rows = [
        "..XXX.....",
        ".X...X....",
        "XX.X.X....",
        "XX...X..X.",
        "..X..X.XX.",
        ".X....X.X.",
        "X.......X.",
        "X.......X.",
        ".X.....X..",
        "..XXXXX...",
    ]
    return [[1 if c == "X" else 0 for c in row] for row in rows]


def draw_pixels(draw, bitmap, x0, y0, size, colour):
    for y, row in enumerate(bitmap):
        for x, v in enumerate(row):
            if v:
                draw.rectangle([x0 + x * size, y0 + y * size, x0 + (x + 1) * size - 1, y0 + (y + 1) * size - 1], fill=colour)


def main():
    grid = crop(load_source())
    big = scale(grid, 2)
    pad = lambda bm: [[0] * len(bm[0])] + bm  # headroom so the walking bob keeps the head
    idle = pad(outline(big))
    asleep = pad(outline(big, eye_closed=True))
    quack = pad(outline(open_beak(big)))
    walk = [with_feet(idle, 0), bob(with_feet(idle, 1))]
    walk_right = [mirror(f) for f in walk]

    images = ROOT / "app" / "images"
    images.mkdir(parents=True, exist_ok=True)
    sprites = {
        "harold_idle": with_feet(idle, 0),
        "harold_asleep": with_feet(asleep, 0),
        "harold_blink_r": mirror(with_feet(asleep, 0)),
        "harold_quack": with_feet(quack, 0),
        "harold_walk_0": walk[0],
        "harold_walk_1": walk[1],
        "harold_walk_r_0": walk_right[0],
        "harold_walk_r_1": walk_right[1],
    }
    for name, bm in sprites.items():
        to_image(bm).save(images / f"{name}_{len(bm[0])}x{len(bm)}.png")
    to_image(icon_bitmap()).save(ROOT / "app" / "icon.png")
    # The icon doubles as the small Harold that traces a walk in Preview.
    to_image(icon_bitmap()).save(images / "harold_mini_10x10.png")

    # Preview sheet: every sprite at 8x on the Flipper orange.
    pad, z = 16, 8
    widths = [len(bm[0]) * z for bm in sprites.values()]
    sheet = Image.new("RGB", (sum(widths) + pad * (len(widths) + 1), max(len(bm) for bm in sprites.values()) * z + 2 * pad), LCD)
    x = pad
    sd = ImageDraw.Draw(sheet)
    for bm in sprites.values():
        draw_pixels(sd, bm, x, pad, z, INK)
        x += len(bm[0]) * z + pad
    (ROOT / "brand").mkdir(exist_ok=True)
    sheet.save(ROOT / "brand" / "sprites-preview.png")

    print("sprites:", ", ".join(sprites), "| icon and preview written")


if __name__ == "__main__":
    main()
