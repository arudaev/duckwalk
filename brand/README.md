# Brand

| File | What it is |
|---|---|
| `banner.png` | README banner, 1536×768: a pen drawing of the Flipper, its screen showing Harold's empty desk, and Harold outside it pushing the pointer |
| `social-preview.png` | The same drawing at 1280×640 with the tagline, for GitHub's social preview |
| `source/harold-16.png` | Harold's original 16 px sprite from VOYAGER // 56K; every Flipper sprite is derived from it |
| `sprites-preview.png` | All Flipper sprites at 8× on the Flipper's orange, for reviewing art changes |
| `tools/make_brand.py` | Draws `app/images/harold_*.png` and `app/icon.png` from the source sprite |

The banner was drawn with an image model. The references were the author's own pen drawings for the style, a photo of the real Flipper running Duckwalk, and Duckwalk's home screen with Harold taken out. Each draft started from those references, never from an earlier draft; the chosen one was checked by hand, cropped to 2:1 and saved from its pixels only, without metadata. The only lettering in the drawing is the name; the social preview's tagline is real text.

**Harold:** a yellow rubber duck with an orange beak and a dot eye. On the Flipper he is 1-bit: an ink outline with a solid beak and eye, 28×29 px. Keep his silhouette when drawing new frames, and regenerate with `python brand/tools/make_brand.py`.

**Colour:** the Flipper's orange `#FF8200` as the one flat colour field; dark ink; white space. No gradients.
