#!/usr/bin/env bash
#
# Render the pictures of the housings in docs/images/: each housing on its
# own, at an angle, and all of them side by side from the front, to scale.
# Run it after changing a housing's design. It needs OpenSCAD, and Python 3
# with Pillow.

set -euo pipefail

cd "$(dirname "$0")"

OUT=../../docs/images
SCENES=(housing_1_28 housing_1_46 speaker_housing_1_46 speaker_housing_1_75)
mkdir -p "$OUT" lineup

for scene in "${SCENES[@]}"; do
    # At an angle, from the front and a little above
    openscad -D 'part="none"' --camera=0,0,-12,58,0,-25,0 --viewall --autocenter \
             --imgsize=1400,1200 --colorscheme=Tomorrow -o "$OUT/$scene.png" "$scene.scad" &
    # From the front, all at the same scale for the side-by-side picture
    openscad -D 'part="none"' --camera=0,-10,-12,0,0,0,420 --projection=o \
             --imgsize=1400,1400 --colorscheme=Tomorrow -o "lineup/$scene.png" "$scene.scad" &
done
wait

python3 - "$OUT" "${SCENES[@]}" <<'PY'
import sys
from PIL import Image, ImageChops

out, scenes = sys.argv[1], sys.argv[2:]

def content_box(im):
    bg = Image.new(im.mode, im.size, im.getpixel((0, 0)))
    return ImageChops.difference(im, bg).getbbox()

def crop(im, box, pad):
    return im.crop((max(box[0] - pad, 0), max(box[1] - pad, 0),
                    min(box[2] + pad, im.width), min(box[3] + pad, im.height)))

# Trim the angled pictures
for s in scenes:
    path = f"{out}/{s}.png"
    im = Image.open(path).convert("RGB")
    crop(im, content_box(im), 20).save(path)

# Side by side: the front views were all rendered at the same scale, so
# they're trimmed to the same height and set next to each other
fronts = [Image.open(f"lineup/{s}.png").convert("RGB") for s in scenes]
boxes = [content_box(f) for f in fronts]
top = min(b[1] for b in boxes)
bottom = max(b[3] for b in boxes)
parts = [crop(f, (b[0], top, b[2], bottom), 30) for f, b in zip(fronts, boxes)]
width = sum(p.width for p in parts)
line = Image.new("RGB", (width, max(p.height for p in parts)), fronts[0].getpixel((0, 0)))
x = 0
for p in parts:
    line.paste(p, (x, 0))
    x += p.width
line.save(f"{out}/housings_to_scale.png")
PY
echo "Rendered the pictures in $OUT"
