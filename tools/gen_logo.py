#!/usr/bin/env python3
"""
Draw the DroneLab logo shown at startup: core/ui/assets/dronelab_logo.png.

"DRONELAB" in the Terminal feature's phosphor green, with a few glitched
slices, inside targeting-style corner brackets with circuit traces off each
side, and a "> SYSTEM ONLINE" prompt under it, all on black with scan lines.
The cursor after the prompt isn't in the picture: the firmware draws it, so it
can blink. This prints where it goes, for SPLASH_CURSOR in core/app/app.c.

The picture is 480x480, and the build scales it to each screen.

It needs Pillow, and two fonts from Google Fonts (both under the SIL Open Font
License), in the folder given with --fonts:

    Orbitron[wght].ttf          https://fonts.google.com/specimen/Orbitron
    ShareTechMono-Regular.ttf   https://fonts.google.com/specimen/Share+Tech+Mono

Usage:
    python3 tools/gen_logo.py --fonts ~/fonts
"""

import argparse
import os
import random

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

SIZE = 480
SS = 3                          # drawn at 3 times the size, then scaled down smoothly
CANVAS = (SIZE * SS, SIZE * SS)
CENTER = (CANVAS[0] // 2, CANVAS[1] // 2)

GREEN = (51, 255, 51)           # COLOR_PHOSPHOR in core/ui/feature_terminal.c
GREEN_BRIGHT = (190, 255, 190)
GREEN_MID = (20, 160, 20)
GREEN_DARK = (10, 90, 10)

TITLE = "DRONELAB"
TITLE_WIDTH = 300               # in the 480 picture, leaving room for the brackets
PROMPT = "> SYSTEM ONLINE"


def load_font(path, size, weight=None):
    font = ImageFont.truetype(path, int(size * SS))
    if weight is not None:
        font.set_variation_by_axes([weight])
    return font


def text_mask(text, font, pos, anchor="mm"):
    mask = Image.new("L", CANVAS, 0)
    ImageDraw.Draw(mask).text(pos, text, font=font, fill=255, anchor=anchor)
    return mask


def fit_font(text, path, width, weight=None):
    """The font size at which text is width pixels wide (in the 480 picture)."""
    size = 100
    for _ in range(3):
        box = text_mask(text, load_font(path, size, weight), CENTER).getbbox()
        size *= width * SS / (box[2] - box[0])
    return load_font(path, size, weight)


def centered(mask, x, y):
    box = mask.getbbox()
    return ImageChops.offset(mask, int(x - (box[0] + box[2]) / 2), int(y - (box[1] + box[3]) / 2))


def paint(img, mask, color):
    img.paste(Image.new("RGB", CANVAS, color), (0, 0), mask)


def glow(img, mask, color, radii, strengths):
    for radius, strength in zip(radii, strengths):
        blurred = mask.filter(ImageFilter.GaussianBlur(radius * SS))
        paint(img, blurred.point(lambda v: int(min(255, v * strength * 1.7))), color)


def gradient(img, mask, stops):
    """Fill mask with colors from top to bottom of its own bounding box."""
    box = mask.getbbox()
    height = box[3] - box[1]
    layer = Image.new("RGB", CANVAS)
    draw = ImageDraw.Draw(layer)
    for y in range(height):
        t = y / max(1, height - 1)
        for (t0, c0), (t1, c1) in zip(stops, stops[1:]):
            if t0 <= t <= t1:
                u = (t - t0) / (t1 - t0)
                color = tuple(int(c0[i] + (c1[i] - c0[i]) * u) for i in range(3))
                break
        draw.line([(0, box[1] + y), (CANVAS[0], box[1] + y)], fill=color)
    img.paste(layer, (0, 0), mask)


def glitch(img, top, bottom, rng):
    """Shift a few thin horizontal slices between top and bottom sideways."""
    out = img.copy()
    for _ in range(4):
        y = rng.randint(top, bottom - 8 * SS)
        height = rng.randint(2, 6) * SS
        dx = rng.choice([-1, 1]) * rng.randint(4, 10) * SS
        band = img.crop((0, y, CANVAS[0], y + height))
        out.paste((0, 0, 0), (0, y, CANVAS[0], y + height))
        out.paste(band, (dx, y))
    return out


def scanlines(img, period=6, darkness=0.3):
    """Dark lines across the picture every period rows, like the Terminal's."""
    lines = Image.new("L", img.size, 0)
    draw = ImageDraw.Draw(lines)
    for y in range(0, img.size[1], period):
        draw.rectangle([0, y + period - 2, img.size[0], y + period - 1], fill=int(255 * darkness))
    return Image.composite(Image.new("RGB", img.size), img, lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--fonts", required=True, help="folder with the two fonts")
    parser.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "core", "ui", "assets",
                                                      "dronelab_logo.png"))
    args = parser.parse_args()
    orbitron = os.path.join(args.fonts, "Orbitron[wght].ttf")
    mono = os.path.join(args.fonts, "ShareTechMono-Regular.ttf")

    rng = random.Random(7)      # the same glitches every time
    img = Image.new("RGB", CANVAS)

    # The title, with a dark ghost to its left, glitched
    title = centered(text_mask(TITLE, fit_font(TITLE, orbitron, TITLE_WIDTH, 900), CENTER),
                     CENTER[0], CENTER[1] - 20 * SS)
    tx0, ty0, tx1, ty1 = title.getbbox()
    glow(img, title, GREEN, (12, 5, 2), (0.35, 0.6, 0.9))
    paint(img, ImageChops.offset(title, -7 * SS, 0).point(lambda v: v // 3), GREEN_DARK)
    gradient(img, title, [(0, GREEN_BRIGHT), (0.5, GREEN), (1, GREEN_MID)])
    img = glitch(img, ty0, ty1, rng)

    # Corner brackets around it, and circuit traces off each side
    pad, arm, line = 16 * SS, 24 * SS, 3 * SS
    fx0, fy0, fx1, fy1 = tx0 - pad, ty0 - pad, tx1 + pad, ty1 + pad
    frame = Image.new("L", CANVAS, 0)
    draw = ImageDraw.Draw(frame)
    for x, y, sx, sy in [(fx0, fy0, 1, 1), (fx1, fy0, -1, 1), (fx0, fy1, 1, -1), (fx1, fy1, -1, -1)]:
        draw.line([(x, y + sy * arm), (x, y), (x + sx * arm, y)], fill=255, width=line)
    mid = (fy0 + fy1) // 2
    for side, x in ((-1, fx0), (1, fx1)):
        points = [(x, mid), (x + side * 14 * SS, mid), (x + side * 24 * SS, mid + 10 * SS),
                  (x + side * 34 * SS, mid + 10 * SS)]
        draw.line(points, fill=255, width=2 * SS)
        end = points[-1]
        draw.ellipse([end[0] - 4 * SS, end[1] - 4 * SS, end[0] + 4 * SS, end[1] + 4 * SS], fill=255)
    glow(img, frame, GREEN, (8, 3), (0.5, 1.0))
    paint(img, frame, GREEN)

    # Small readouts above the brackets
    small = load_font(mono, 14)
    paint(img, text_mask("SYS.0x3F7A", small, (fx0, fy0 - 10 * SS), anchor="ls"), GREEN_MID)
    paint(img, text_mask("LINK OK", small, (fx1, fy0 - 10 * SS), anchor="rs"), GREEN_MID)

    # The prompt under it all, a little left of center to leave room for the cursor
    prompt_font = load_font(mono, 22)
    char_w = prompt_font.getlength("M")
    prompt = text_mask(PROMPT, prompt_font, (CENTER[0] - char_w, fy1 + 34 * SS))
    glow(img, prompt, GREEN, (6, 2), (0.5, 1.0))
    paint(img, prompt, GREEN)

    img = scanlines(img.resize((SIZE, SIZE), Image.LANCZOS))
    img.save(args.out, optimize=True)

    # Where the firmware's cursor goes: a block one character wide, a space
    # after the prompt, as tall as its capitals
    px0, py0, px1, py1 = prompt.getbbox()
    cap = text_mask("M", prompt_font, (CENTER[0], fy1 + 34 * SS)).getbbox()
    x = (px1 + char_w * 0.6) / SS
    print(f"Wrote {args.out}")
    print(f"SPLASH_CURSOR: x {x:.0f}, y {cap[1] / SS:.0f}, w {char_w * 0.65 / SS:.0f}, h {(cap[3] - cap[1]) / SS:.0f}"
          f" (in the {SIZE}x{SIZE} picture)")


if __name__ == "__main__":
    main()
