# Brand

| File | What it is |
|---|---|
| `banner.png` | README banner, 1536×1024: a pen drawing of Harold nudging the pointer from the Flipper to a laptop |
| `social-preview.png` | The same drawing cropped to 1280×640 for GitHub's social preview and the website's link preview (copied to `site/`) |
| `source/harold-16.png` | Harold's original 16 px sprite from VOYAGER // 56K; every Flipper sprite is derived from it |
| `sprites-preview.png` | All Flipper sprites at 8× on the Flipper's orange, for reviewing art changes |
| `tools/make_brand.py` | Draws `app/images/harold_*.png` and `app/icon.png` from the source sprite |

The banner was drawn with an image model, using the author's own pen drawings and the Flipper's outline as references, then checked by hand for errors (the D-pad's OK button was added) and saved without metadata.

**Harold:** a yellow rubber duck with an orange beak and a dot eye. On the Flipper he is 1-bit: an ink outline with a solid beak and eye, 28×29 px. Keep his silhouette when drawing new frames, and regenerate with `python brand/tools/make_brand.py`.

**Colour:** the Flipper's orange `#FF8200` as the one flat colour field; dark ink; white space. No gradients.
