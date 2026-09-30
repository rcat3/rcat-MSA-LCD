#!/usr/bin/env python3
"""
Generate the lv_conf.h files for each platform from the LVGL submodule's
lv_conf_template.h, with this project's settings applied.

Run it after moving the LVGL submodule to a new version:
    python3 tools/gen_lv_conf.py
"""

import os
import re
import sys

REPO = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
TEMPLATE = os.path.join(REPO, "third_party", "lvgl", "lv_conf_template.h")

# Settings for every platform
COMMON = {
    "LV_USE_STDLIB_MALLOC": ("LV_STDLIB_CLIB", "C library malloc (can use PSRAM on the ESP32)"),
    "LV_DEF_REFR_PERIOD": ("10", "refresh often, for smooth swiping"),
    "LV_USE_GIF": ("1", "animated GIFs"),
}

PLATFORMS = {
    "pico": {},
    "esp-idf": {
        "LV_FONT_MONTSERRAT_24": ("1", "for larger screens"),
        # Images on the SD card
        "LV_USE_FS_STDIO": ("1", "SD card files, drive letter S"),
        "LV_FS_STDIO_LETTER": ("'S'", None),
        "LV_USE_LODEPNG": ("1", "PNG"),
        "LV_USE_BMP": ("1", "BMP"),
        "LV_USE_LIBJPEG_TURBO": ("1", "JPEG, including progressive JPEG"),
    },
}


def generate(platform, settings):
    with open(TEMPLATE) as f:
        text = f.read()

    version = re.search(r"Configuration file for (v[\d.]+)", text).group(1)
    text, n = re.subn(r'^#if 0 /\* Set this to "1" to enable content \*/',
                      '#if 1 /* Set this to "1" to enable content */', text, count=1, flags=re.M)
    assert n == 1, "couldn't find the enable switch"

    notes = []
    for name, (value, note) in {**COMMON, **settings}.items():
        text, n = re.subn(rf"^(\s*#define {name}\s+)\S.*?(\s*(/\*.*)?)$",
                          lambda m: m.group(1) + value + m.group(2), text, count=1, flags=re.M)
        if n != 1:
            sys.exit(f"{name} isn't in the template")
        if note:
            notes.append(f" *   {name} = {value}: {note}")

    header = (f"/**\n * @file lv_conf.h\n"
              f" * LVGL configuration for the {platform} platform, generated from the LVGL\n"
              f" * {version} lv_conf_template.h by tools/gen_lv_conf.py. Edit that script\n"
              f" * rather than this file. Changes from the template:\n"
              + "\n".join(notes) + "\n"
              f" * The RGB565 byte swap the panels need is done in the display flush callback.\n */\n\n")
    out = os.path.join(REPO, "platforms", platform, "config", "lv_conf.h")
    with open(out, "w") as f:
        f.write(header + text)
    print(f"{out}: {version}")


def main():
    for platform, settings in PLATFORMS.items():
        generate(platform, settings)


if __name__ == "__main__":
    main()
