"""Make the Apps Catalog screenshots from the simulator's 128x64 renders.

The catalog takes 512x256 PNGs in exactly two colours, the qFlipper orange
(254,138,44) and black, scaled 4x with no smoothing. The simulator draws in
a slightly different orange, so this redraws each render pixel by pixel.

    python scripts/catalog_screens.py [renders dir]   (default docs/screens)

Writes app/screenshots/ss0.png and on; the first is the catalog's preview.
"""
import sys
from pathlib import Path

from PIL import Image

SCREENS = ["home-awake", "home-wandering", "settings", "remote-media", "preview", "pairing-usb-all-set"]
ORANGE = (254, 138, 44)
BLACK = (0, 0, 0)


def main(argv):
    src = Path(argv[0] if argv else "docs/screens")
    out = Path("app/screenshots")
    out.mkdir(exist_ok=True)
    for old in out.glob("ss*.png"):
        old.unlink()
    for i, name in enumerate(SCREENS):
        im = Image.open(src / f"{name}.png").convert("RGB")
        if im.size != (128, 64):
            sys.exit(f"{name}.png is {im.size}, not 128x64")
        two = Image.new("RGB", im.size)
        two.putdata([BLACK if sum(p) < 384 else ORANGE for p in im.getdata()])
        two.resize((512, 256), Image.NEAREST).save(out / f"ss{i}.png", optimize=True)
        print(f"ss{i}.png  {name}")


if __name__ == "__main__":
    main(sys.argv[1:])
