#!/usr/bin/env python3
"""Make the Terminal feature's fonts from LVGL's unscii 8.

unscii is a public domain 8x8 pixel font in the style of old computer
terminals. LVGL only has it at 8 and 16 px, which is small on a mask, so this
blows each pixel up into a square block, keeping it crisp and blocky at any
size:

    python3 tools/gen_terminal_font.py

It writes core/ui/terminal_font.c with terminal_font_1 to terminal_font_6
(8 to 48 px). Run it again only to change which sizes there are.
"""
import os
import re

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = os.path.join(REPO, "third_party", "lvgl", "src", "font", "lv_font_unscii_8.c")
OUT = os.path.join(REPO, "core", "ui", "terminal_font.c")
SCALES = range(1, 7)


def read_font():
    text = open(SOURCE).read()
    bitmap_text = text[text.index("glyph_bitmap[] = {"):text.index("};", text.index("glyph_bitmap[] = {"))]
    bitmap = [int(v, 16) for v in re.findall(r"0x([0-9a-fA-F]{2})", bitmap_text)]
    glyphs = []
    for m in re.finditer(r"\{\.bitmap_index = (\d+), \.adv_w = (\d+), \.box_w = (\d+), \.box_h = (\d+), "
                         r"\.ofs_x = (-?\d+), \.ofs_y = (-?\d+)\}", text):
        glyphs.append(tuple(int(v) for v in m.groups()))
    line_height = int(re.search(r"\.line_height = (\d+)", text).group(1))
    return bitmap, glyphs, line_height


def glyph_pixels(bitmap, index, w, h):
    """The glyph's pixels, row by row. The bits run on from row to row."""
    bits = []
    for byte in bitmap[index:index + (w * h + 7) // 8]:
        bits.extend((byte >> (7 - i)) & 1 for i in range(8))
    return [bits[r * w:(r + 1) * w] for r in range(h)]


def pack(rows):
    bits = [b for row in rows for b in row]
    bits += [0] * (-len(bits) % 8)
    return [sum(bits[i + j] << (7 - j) for j in range(8)) for i in range(0, len(bits), 8)]


def font(bitmap, glyphs, line_height, s):
    data, dscs = [], []
    for (index, adv_w, w, h, ox, oy) in glyphs:
        start = len(data)
        if w and h:
            rows = glyph_pixels(bitmap, index, w, h)
            big = [[b for b in row for _ in range(s)] for row in rows for _ in range(s)]
            data += pack(big)
        dscs.append((start, adv_w * s, w * s, h * s, ox * s, oy * s))
    name = f"terminal_font_{s}"
    out = [f"/* {8 * s} px: each pixel of unscii 8 as a {s}x{s} block */" if s > 1 else "/* 8 px: unscii 8 as it is */"]
    out.append(f"static const uint8_t {name}_bitmap[] = {{")
    for i in range(0, len(data), 16):
        out.append("    " + ", ".join(f"0x{b:02x}" for b in data[i:i + 16]) + ",")
    out.append("};\n")
    out.append(f"static const lv_font_fmt_txt_glyph_dsc_t {name}_glyphs[] = {{")
    for d in dscs:
        out.append("    {.bitmap_index = %d, .adv_w = %d, .box_w = %d, .box_h = %d, .ofs_x = %d, .ofs_y = %d}," % d)
    out.append("};\n")
    out.append(f"static const lv_font_fmt_txt_dsc_t {name}_dsc = {{")
    out.append(f"    .glyph_bitmap = {name}_bitmap,")
    out.append(f"    .glyph_dsc = {name}_glyphs,")
    out.append("    .cmaps = cmaps,")
    out.append("    .cmap_num = 1,")
    out.append("    .bpp = 1,")
    out.append("};\n")
    out.append(f"const lv_font_t {name} = {{")
    out.append("    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,")
    out.append("    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,")
    out.append(f"    .line_height = {line_height * s},")
    out.append("    .base_line = 0,")
    out.append("    .subpx = LV_FONT_SUBPX_NONE,")
    out.append("    .static_bitmap = 1,")
    out.append(f"    .dsc = &{name}_dsc,")
    out.append("};\n")
    return "\n".join(out)


def main():
    bitmap, glyphs, line_height = read_font()
    assert len(glyphs) == 97, len(glyphs)       # the reserved one, then U+0020 to U+007F
    parts = [
        "/*",
        " * The Terminal feature's fonts: LVGL's unscii 8 (public domain) with each",
        " * pixel blown up into a square block, by tools/gen_terminal_font.py. Edit",
        " * the script rather than this file.",
        " */",
        '#include "lvgl.h"',
        "",
        "#if RCAT_FEATURE_TERMINAL",
        "",
        "/* U+0020 to U+007F */",
        "static const lv_font_fmt_txt_cmap_t cmaps[] = {",
        "    {.range_start = 32, .range_length = 96, .glyph_id_start = 1, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY},",
        "};",
        "",
    ]
    for s in SCALES:
        parts.append(font(bitmap, glyphs, line_height, s))
    parts.append("#endif")
    with open(OUT, "w") as f:
        f.write("\n".join(parts) + "\n")
    print(f"{OUT}: scales {list(SCALES)}")


if __name__ == "__main__":
    main()
