"""Harold style candidates for issue #34, drawn as 1-bit pixel art.

Each candidate is built from a few shapes taken from the README banner's
Harold (a round head, a cone beak, a round body with an upturned tail, a
wing curve), outlined one pixel thick, then fixed up by hand in this file:
the wing, the beak and the eye are placed pixel by pixel.

    python brand/tools/harold_candidates.py

Writes brand/proposals/: one 1-bit PNG per candidate and pose, and
harold-style-sheet.png comparing them with today's sprites. Nothing here is
used by the app until a style is chosen.
"""
import sys
from PIL import Image, ImageDraw

W, H = 36, 30


def inside(x, y, shapes):
    return any(s(x + 0.5, y + 0.5) for s in shapes)


def ellipse(cx, cy, rx, ry):
    return lambda x, y: ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0


def poly(points):
    def f(x, y):
        c = False
        n = len(points)
        for i in range(n):
            x1, y1 = points[i]
            x2, y2 = points[(i + 1) % n]
            if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1:
                c = not c
        return c
    return f


def draw(shaded, pose="idle"):
    head = ellipse(23, 9, 7.6, 7.4)
    body = ellipse(16, 20.5, 13.2, 7.6)
    tail = poly([(2.5, 11), (9, 15.5), (5, 21)])
    if pose == "quack":
        beak_parts = [poly([(29.5, 8.0), (35.5, 8.8), (30, 10.6)]), poly([(30, 11.6), (34.6, 13.0), (29.5, 13.6)])]
    else:
        beak_parts = [poly([(29.5, 8.6), (35.6, 10.6), (29.5, 12.8)])]
    duck = [head, body, tail]
    g = [[0] * W for _ in range(H)]
    fill = [[inside(x, y, duck) for x in range(W)] for y in range(H)]
    for y in range(H):
        for x in range(W):
            if not fill[y][x]:
                continue
            edge = any(not (0 <= x + dx < W and 0 <= y + dy < H and fill[y + dy][x + dx])
                       for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
            if edge:
                g[y][x] = 1
            elif shaded:
                # A dithered crescent along the bottom of the body.
                below = sum(1 for k in range(1, 4) if y + k < H and fill[y + k][x])
                if below < 3 and y > 21 and (x + y) % 2 == 0:
                    g[y][x] = 1
    # Beak, solid.
    for y in range(H):
        for x in range(W):
            if any(b(x + 0.5, y + 0.5) for b in beak_parts) and not fill[y][x]:
                g[y][x] = 1
    # Wing: the lower half of a small ellipse, open at the top like the banner's.
    for x in range(10, 25):
        t = (x - 17.0) / 7.0
        if abs(t) <= 1:
            y = int(round(18.5 + 4.0 * (1 - t * t) ** 0.5))
            if fill[y][x]:
                g[y][x] = 1
    for y in range(17, 20):
        for x in (10, 24):
            if fill[y][x]:
                g[y][x] = 1
    # Eye with a highlight, or closed.
    if pose == "asleep":
        for x in range(23, 27):
            g[8][x] = 1
    else:
        for y in (6, 7, 8):
            for x in (24, 25):
                g[y][x] = 1
        g[6][24] = 0
    # Feet.
    for x in list(range(10, 14)) + list(range(19, 23)):
        g[28][x] = 1
    return g


def to_image(g, scale=1, ink=(0, 0, 0), paper=(255, 130, 0)):
    im = Image.new("RGB", (W * scale, H * scale), paper)
    d = ImageDraw.Draw(im)
    for y, row in enumerate(g):
        for x, v in enumerate(row):
            if v:
                d.rectangle([x * scale, y * scale, x * scale + scale - 1, y * scale + scale - 1], fill=ink)
    return im


def main():
    from pathlib import Path

    root = Path(__file__).resolve().parents[2]
    out = root / "brand" / "proposals"
    out.mkdir(parents=True, exist_ok=True)
    poses = ["idle", "quack", "asleep"]
    candidates = {"a": False, "b": True}  # a: clean line, b: dithered shading
    for name, shaded in candidates.items():
        for pose in poses:
            to_image(draw(shaded, pose), 1, (0, 0, 0), (255, 255, 255)).convert("1").save(
                out / f"harold-{name}-{pose}_{W}x{H}.png")

    # The comparison sheet: today's sprites, then each candidate, 6x.
    z, pad = 6, 14
    now = [root / "app" / "images" / f for f in
           ("harold_idle_28x29.png", "harold_quack_28x29.png", "harold_asleep_28x29.png")]
    rows = [("Now", [Image.open(f).convert("1") for f in now])]
    for label, shaded in (("A: clean line", False), ("B: shaded", True)):
        rows.append((label, [to_image(draw(shaded, p), 1, (0, 0, 0), (255, 255, 255)).convert("1") for p in poses]))
    cell_w, cell_h = W * z + pad, H * z + pad
    sheet = Image.new("RGB", (110 + cell_w * 3 + pad, pad + cell_h * len(rows) + 20), (40, 40, 40))
    d = ImageDraw.Draw(sheet)
    for c, pose in enumerate(poses):
        d.text((110 + pad + c * cell_w, 4), pose, fill=(230, 230, 230))
    for r, (label, images) in enumerate(rows):
        y = 20 + r * cell_h
        d.text((pad, y + cell_h // 2 - 6), label, fill=(230, 230, 230))
        for c, im in enumerate(images):
            tile = Image.new("RGB", (W * z, H * z), (255, 130, 0))
            big = im.resize((im.width * z, im.height * z), Image.NEAREST)
            ink = Image.new("RGB", big.size, (0, 0, 0))
            tile.paste(ink, (0, (H - im.height) * z), big.point(lambda v: 255 - v).convert("1"))
            sheet.paste(tile, (110 + pad + c * cell_w, y))
    sheet.save(out / "harold-style-sheet.png")
    print("candidates and sheet written to", out)


if __name__ == "__main__":
    main()
