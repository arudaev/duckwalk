"""Harold style candidates for issue #34, drawn as 1-bit pixel art.

Harold is a rubber duck, the bath toy: a big round head sitting low on a
chubby body with a flat bottom it floats on, a small upturned tail, a flat
wide bill, a big glossy eye, a little wing and no legs. Each candidate builds
that outline from a few shapes (head, body, tail, bill), one pixel thick, and
places the eye, wing and bill line pixel by pixel. Three styles:

    A  line    the outline only, like the README banner
    B  dither  a dithered shadow along the bottom
    C  comic   bold line weight on the shadow side, two-tone halftone
               shading, and a gloss where the light catches the rubber

    python brand/tools/harold_candidates.py

Writes brand/proposals/: one 1-bit PNG per candidate and pose, and
harold-style-sheet.png comparing them with today's sprites. Nothing here is
used by the app until a style is chosen.
"""
import math
from pathlib import Path

from PIL import Image, ImageDraw

W, H = 36, 30
STYLES = {"a": "line", "b": "dither", "c": "comic"}
POSES = ["idle", "quack", "asleep"]


def ellipse(cx, cy, rx, ry):
    return lambda x, y: ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0


def poly(points):
    def f(x, y):
        c = False
        for i in range(len(points)):
            x1, y1 = points[i]
            x2, y2 = points[(i + 1) % len(points)]
            if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1:
                c = not c
        return c
    return f


def mask(shapes):
    return [[any(s(x + 0.5, y + 0.5) for s in shapes) for x in range(W)] for y in range(H)]


def out(m, x, y):
    return not (0 <= x < W and 0 <= y < H and m[y][x])


# The shape, facing right. The body is an ellipse cut flat at the waterline.
BASE_Y = 27
HEAD = (21.5, 10.5, 7.6, 7.4)  # centre x, centre y, radius x, radius y
BODY = (15.5, 20.0, 14.3, 8.4)
head = ellipse(*HEAD)
body_round = ellipse(*BODY)
body = lambda x, y: body_round(x, y) and y < BASE_Y  # noqa: E731
tail = poly([(0.8, 12.5), (7.5, 15.5), (8.5, 19.5), (2.6, 18.6)])


def bill(pose):
    if pose == "quack":
        return [ellipse(30.4, 10.4, 3.6, 1.4), ellipse(29.8, 14.4, 3.2, 1.3)]
    return [ellipse(30.0, 12.3, 3.6, 2.1)]


def draw(style, pose="idle"):
    g = [[0] * W for _ in range(H)]
    duck = mask([head, body, tail])
    head_m = mask([head])

    for y in range(H):
        for x in range(W):
            if not duck[y][x]:
                continue
            if any(out(duck, x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                g[y][x] = 1
                continue
            if style == "dither":
                # A dithered shadow along the bottom of the body.
                if y >= BASE_Y - 3 and (x + y) % 2 == 0:
                    g[y][x] = 1
            elif style == "comic":
                # Bold line weight on the shadow side: the left and the bottom.
                if out(duck, x - 2, y) or out(duck, x, y + 2):
                    g[y][x] = 1
                    continue
                # Halftone, light from the top right: dots, then a checker.
                if head_m[y][x]:
                    continue  # the head stays clean; the gloss carries it
                cx, cy, rx, ry = BODY
                shade = (y - cy) / ry - 0.6 * (x - cx) / rx
                if shade > 0.85 and (x + y) % 2 == 0:
                    g[y][x] = 1
                elif shade > 0.5 and x % 2 == 0 and y % 2 == 0:
                    g[y][x] = 1

    # The bill: solid (it's the orange part), with a paper mouth line.
    bill_m = mask(bill(pose))
    for y in range(H):
        for x in range(W):
            if bill_m[y][x] and not (duck[y][x] and not any(out(duck, x + dx, y + dy)
                                                             for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))):
                g[y][x] = 1
    if pose != "quack":
        for x in range(29, 34):
            g[12][x] = 0

    # The eye: big, with a highlight; a happy curve when asleep.
    if pose == "asleep":
        for x, y in ((22, 7), (23, 8), (24, 8), (25, 7)):
            g[y][x] = 1
    else:
        for y in range(6, 10):
            for x in range(23, 26):
                g[y][x] = 1
        g[6][23] = 0

    # The wing: a small rounded flap with its tip lifted at the back.
    for x in range(10, 22):
        t = (x - 15.5) / 5.5
        y = int(round(19.0 + 3.2 * math.sqrt(max(0.0, 1 - t * t))))
        g[y][x] = 1
    for x, y in ((10, 18), (9, 17), (10, 17), (21, 18), (21, 17)):
        g[y][x] = 1

    if style == "comic":
        # Gloss: the outline breaks where the light catches the rubber.
        hx, hy = HEAD[0], HEAD[1]
        for y in range(H):
            for x in range(W):
                dx, dy = x + 0.5 - hx, hy - (y + 0.5)
                if 6.5 <= math.hypot(dx, dy) <= 8.4 and 38 <= math.degrees(math.atan2(dy, dx)) <= 62:
                    g[y][x] = 0
        if pose == "quack":
            # Shout marks.
            for x, y in ((30, 4), (31, 3), (32, 2), (34, 6), (35, 5)):
                g[y][x] = 1
    return g


def to_image(g, ink=(0, 0, 0), paper=(255, 255, 255)):
    im = Image.new("RGB", (W, H), paper)
    for y, row in enumerate(g):
        for x, v in enumerate(row):
            if v:
                im.putpixel((x, y), ink)
    return im


def main():
    root = Path(__file__).resolve().parents[2]
    folder = root / "brand" / "proposals"
    folder.mkdir(parents=True, exist_ok=True)
    for old in folder.glob("harold-*_*.png"):
        old.unlink()
    for name, style in STYLES.items():
        for pose in POSES:
            to_image(draw(style, pose)).convert("1").save(folder / f"harold-{name}-{pose}_{W}x{H}.png")

    # The comparison sheet: today's sprites, then each candidate, 6x, on the
    # Flipper's orange.
    z, pad = 6, 14
    now = [root / "app" / "images" / f for f in
           ("harold_idle_28x29.png", "harold_quack_28x29.png", "harold_asleep_28x29.png")]
    rows = [("Now", [Image.open(f).convert("1") for f in now])]
    labels = {"line": "A: clean line", "dither": "B: shaded", "comic": "C: comic"}
    for style in STYLES.values():
        rows.append((labels[style], [to_image(draw(style, p)).convert("1") for p in POSES]))
    cell_w, cell_h = W * z + pad, H * z + pad
    sheet = Image.new("RGB", (110 + cell_w * 3 + pad, 20 + cell_h * len(rows)), (40, 40, 40))
    d = ImageDraw.Draw(sheet)
    for c, pose in enumerate(POSES):
        d.text((110 + c * cell_w, 4), pose, fill=(230, 230, 230))
    for r, (label, images) in enumerate(rows):
        y = 20 + r * cell_h
        d.text((pad, y + cell_h // 2 - 6), label, fill=(230, 230, 230))
        for c, im in enumerate(images):
            tile = Image.new("RGB", (W * z, H * z), (255, 130, 0))
            big = im.resize((im.width * z, im.height * z), Image.NEAREST)
            ink = Image.new("RGB", big.size, (0, 0, 0))
            tile.paste(ink, (0, (H - im.height) * z), big.point(lambda v: 255 - v).convert("1"))
            sheet.paste(tile, (110 + c * cell_w, y))
    sheet.save(folder / "harold-style-sheet.png")
    print("candidates and sheet written to", folder)


if __name__ == "__main__":
    main()
