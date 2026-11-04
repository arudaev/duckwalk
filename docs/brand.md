# Brand

Duckwalk follows its author's pen-and-paper style and keeps the Flipper's own orange: thin, slightly wobbly ink lines on white paper, generous white space, and one flat orange field. Harold brings the only other colour, his yellow. The [banner](../brand/banner.png) is the reference for everything we draw or publish.

## Colours

| Token | Light | Dark | Use |
|---|---|---|---|
| `paper` | `#fdfcf8` | `#17140f` | Background, and the fill inside every drawing |
| `ink` | `#1f1b16` | `#efe8dc` | Lines and text |
| `soft` | `#6b6359` | `#a99f91` | Hints, footer |
| `faint` | `#e7e1d6` | `#2c2720` | Code backgrounds, the page of code on the laptop |
| `orange` | `#fe8a2c` | `#f08024` | The one flat field, the main button, the pointer's dotted trail |
| `duck` | `#ffd23f` | `#ffd23f` | Harold and only Harold |

Real Flipper screens stay orange with black pixels in both themes and are never redrawn: they come from `sim/`.

## Type

- **Silkscreen**, regular weight only (the bold W is unreadable): the name, headings, buttons. Short words only.
- **System sans** for sentences.
- Silkscreen is self-hosted under the SIL Open Font License (`site/fonts/`).

## Drawings

- Ink outlines with a slight wobble (the `wobble` SVG filter), filled with paper, no gradients or shadows.
- **The Flipper** is qFlipper's drawing (`flipper.svg`, GPL-3.0, by Flipper Devices), re-coloured: body in `paper`, outlines in `ink`. The original is kept in `site/flipper-qflipper.svg`. Its screen sits at x 93, y 26, 128 × 64 in the 360 × 156 drawing.
- **The laptop** is our own line drawing; its screen is the demo canvas.
- **Harold** in the drawings is the duck from the banner (`site/harold-walk.png`); on the Flipper he is the app's pixel sprite.
- Movement is a dotted line, like the banner's path.

## Rules

- **Show, don't explain.** A demo or a real screen beats a paragraph. One line of text per idea.
- **Nothing overlaps.** Check every page at 375 px and at desktop width, light and dark, on localhost, before a pull request.
- **No generated images without real references**, and no metadata or watermarks in what ships.
- **Not affiliated.** Flipper Zero is a trademark of Flipper Devices Inc. We show the device to show where Harold runs, never their logo on its own.
