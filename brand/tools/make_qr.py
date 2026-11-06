"""Draw the QR code the connection check shows next to "get the desktop app".

The Flipper SDK has no QR encoder, and the address never changes, so the code
is drawn once here and committed as a 1-bit image:

    pip install qrcode pillow
    python brand/tools/make_qr.py

Writes app/images/qr_site_54x54.png: the desktop app section of the Duckwalk
site, 25x25 modules with a
one-module quiet zone, each module 2x2 pixels (black = pixel on).

The scheme and host are written in capitals: QR codes store capitals and a
few symbols in a denser mode, which keeps the code at version 2 (25x25)
instead of 3. Browsers treat them the same; the path stays lower case because
GitHub Pages paths are case-sensitive.
"""

from pathlib import Path

import qrcode
from PIL import Image

URL = "HTTPS://ARUDAEV.GITHUB.IO/duckwalk/#app"
SCALE = 2

ROOT = Path(__file__).resolve().parents[2]


def main() -> None:
    qr = qrcode.QRCode(error_correction=qrcode.constants.ERROR_CORRECT_L, border=1, box_size=1)
    qr.add_data(URL)
    qr.make(fit=True)
    assert qr.version == 2, f"expected version 2, got {qr.version}"
    matrix = qr.get_matrix()  # True = dark module, quiet zone included
    size = len(matrix) * SCALE
    img = Image.new("1", (size, size), 1)
    for y, row in enumerate(matrix):
        for x, dark in enumerate(row):
            if dark:
                for dy in range(SCALE):
                    for dx in range(SCALE):
                        img.putpixel((x * SCALE + dx, y * SCALE + dy), 0)
    out = ROOT / "app" / "images" / f"qr_site_{size}x{size}.png"
    img.save(out)
    print(f"wrote {out.relative_to(ROOT)} ({len(matrix)} modules)")


if __name__ == "__main__":
    main()
