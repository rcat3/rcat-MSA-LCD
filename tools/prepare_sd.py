#!/usr/bin/env python3
"""
Prepare images for a display's SD card.

The display can load PNG, JPG, BMP and GIF files from the card as they are,
but it can't resize GIFs, and LVGL shows flashes in GIFs that use
transparency. This writes copies that are ready to go:

- Animated GIFs are resized for the screen and re-encoded without
  transparency (see gif_for_lvgl in img2c.py).
- Static images are resized for the screen and saved as PNG, which looks
  sharper and loads faster than having the display scale them.

Sizing works the same as for the built-in images: images up to 240x240 are
scaled by the screen size / 240, and bigger images are fitted to the screen.
Each file gets a small marker ("rcat-msa-lcd:prepared:<size>") so the display
knows it's already sized and doesn't scale it again.

Usage:
    python3 tools/prepare_sd.py imgs/*.gif my_photos/*.jpg --size 412 --out /media/me/SDCARD/images

--size is the screen size: 412 for the ESP32-S3-Touch-LCD-1.46.

Needs Pillow:  pip install pillow
"""

import argparse
import os
import sys

from PIL import Image, PngImagePlugin

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from img2c import gif_for_lvgl, scaled_size  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("images", nargs="+")
    ap.add_argument("--size", type=int, required=True, help="screen width/height in pixels, e.g. 412")
    ap.add_argument("--out", required=True, help="output folder, e.g. the card's images folder")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)

    for path in args.images:
        name = os.path.splitext(os.path.basename(path))[0]
        try:
            img = Image.open(path)
        except OSError as e:
            print(f"skipping {path}: {e}")
            continue

        marker = f"rcat-msa-lcd:prepared:{args.size}"
        w, h = scaled_size(*img.size, args.size)
        if img.format == "GIF":
            out_path = os.path.join(args.out, name + ".gif")
            with open(out_path, "wb") as f:
                f.write(gif_for_lvgl(img, (w, h), comment=marker.encode()))
        else:
            out_path = os.path.join(args.out, name + ".png")
            img = img.convert("RGBA")
            if (w, h) != img.size:
                img = img.resize((w, h), Image.LANCZOS)
            info = PngImagePlugin.PngInfo()
            info.add_text("Comment", marker)
            img.save(out_path, pnginfo=info)

        print(f"{out_path}: {w}x{h}")


if __name__ == "__main__":
    main()
