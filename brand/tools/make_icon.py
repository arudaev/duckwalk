"""Draw the desktop app's icon: Harold in colour, from his master drawing.

The Flipper sprites are 1-bit outlines; on a computer Harold gets his real
colours (docs/brand.md): `duck` yellow inside the outline, an orange beak,
`ink` lines and eye, and a white glint in the eye. The pixels stay sharp: each
grid cell becomes a square block, never smoothed.

    pip install pillow
    python brand/tools/make_icon.py

The tray icon's frames come from make_tray.py. Writes:
    desktop/app/assets/duckwalk.ico        16 to 256 px, for the Windows exe
    .claude-plugin/icon.png                the Claude Code plugin's icon, 512 px
"""

from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "brand" / "source" / "harold-24.txt"
OUT = ROOT / "desktop" / "app" / "assets"

INK = (0x1F, 0x1B, 0x16, 255)
DUCK = (0xFF, 0xD2, 0x3F, 255)
BEAK = (0xFE, 0x8A, 0x2C, 255)
GLINT = (0xFD, 0xFC, 0xF8, 255)
CLEAR = (0, 0, 0, 0)


def grid() -> list[list[bool]]:
    rows = [line.strip() for line in SOURCE.read_text().splitlines() if line.strip()]
    return [[c == "#" for c in row] for row in rows]


def colours(ink: list[list[bool]]) -> list[list[tuple]]:
    """Ink stays ink; every enclosed area gets its colour: the biggest is the
    body, one right of the head is the beak, a single cell is the eye's glint."""
    h, w = len(ink), len(ink[0])
    label = [[-1] * w for _ in range(h)]
    areas = []
    for y in range(h):
        for x in range(w):
            if ink[y][x] or label[y][x] != -1:
                continue
            cells, stack, edge = [], [(x, y)], False
            label[y][x] = len(areas)
            while stack:
                cx, cy = stack.pop()
                cells.append((cx, cy))
                edge |= cx in (0, w - 1) or cy in (0, h - 1)
                for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                    if 0 <= nx < w and 0 <= ny < h and not ink[ny][nx] and label[ny][nx] == -1:
                        label[ny][nx] = len(areas)
                        stack.append((nx, ny))
            areas.append((cells, edge))
    body = max((i for i, (cells, edge) in enumerate(areas) if not edge), key=lambda i: len(areas[i][0]))
    out = [[INK if ink[y][x] else CLEAR for x in range(w)] for y in range(h)]
    for i, (cells, edge) in enumerate(areas):
        if edge:
            continue  # outside Harold
        if i == body:
            fill = DUCK
        elif len(cells) <= 2:
            fill = GLINT
        elif min(x for x, _ in cells) >= w * 3 // 4:
            fill = BEAK
        else:
            fill = DUCK
        for x, y in cells:
            out[y][x] = fill
    return out


def render(pixels: list[list[tuple]], size: int, grey: bool = False) -> Image.Image:
    """Harold centred on a square canvas, each cell a whole number of pixels."""
    h, w = len(pixels), len(pixels[0])
    if size < max(w, h):
        # Smaller than the drawing (16 px): draw at 24 and scale down once.
        return render(pixels, max(w, h), grey).resize((size, size), Image.LANCZOS)
    cell = size // max(w, h)
    img = Image.new("RGBA", (size, size), CLEAR)
    ox, oy = (size - w * cell) // 2, (size - h * cell) // 2
    for y, row in enumerate(pixels):
        for x, c in enumerate(row):
            if c[3] == 0:
                continue
            if grey:
                level = (c[0] * 3 + c[1] * 6 + c[2]) // 10
                c = (level, level, level, 140)
            for dy in range(cell):
                for dx in range(cell):
                    img.putpixel((ox + x * cell + dx, oy + y * cell + dy), c)
    return img


def main() -> None:
    pixels = colours(grid())
    OUT.mkdir(parents=True, exist_ok=True)
    sizes = [16, 24, 32, 48, 64, 128, 256]
    big = render(pixels, 256)
    big.save(OUT / "duckwalk.ico", sizes=[(s, s) for s in sizes], append_images=[render(pixels, s) for s in sizes])
    # The Claude Code plugin's icon (.claude-plugin/icon.png): 512 px, square.
    render(pixels, 512).save(ROOT / ".claude-plugin" / "icon.png")
    print(f"wrote {OUT.relative_to(ROOT)}: duckwalk.ico")


if __name__ == "__main__":
    main()
