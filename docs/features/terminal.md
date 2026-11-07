# Terminal

*Boards with WiFi: the ESP32-S3 boards*

The Terminal shows what's typed on the [Phone Remote](phone-remote.md) like an old green-screen terminal: blocky green letters that type themselves out behind a block cursor, which blinks once they're done, with faint scan lines over the screen.  It shows a `>` prompt until something's sent.

The letter size is picked on the phone.  *Auto* uses the biggest letters the whole message fits in (so a word or two fills the screen), down to 16 px, or pick one of 8, 16, 24, 32, 40 or 48 px.  A message too long for the size scrolls like a terminal, keeping the newest lines on screen.

The display only has plain letters, numbers and punctuation, so curly quotes and dashes become straight ones, and anything else (emoji, accents) becomes `?`.

It can be picked to show when the display turns on, in [Settings](settings.md#startup).

The Terminal's font is LVGL's 8x8 unscii font with each pixel drawn as a square block.  `tools/gen_terminal_font.py` makes `core/ui/terminal_font.c` from it.
