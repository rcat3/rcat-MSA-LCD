# Settings

*All boards*

Settings that apply to the whole display, one per page.  Settings that belong to a single feature, like the [Voice Ring](kitt-and-voice-ring.md#voice-ring)'s color or the [Sound](sound.md) volume, are on that feature's own pages.

Settings are saved about two seconds after you change them, and are restored when the display is powered back on.  They also survive flashing new firmware.

## Brightness

How bright the screen is, and the battery's voltage under it.  On the ESP32-S3-Touch-AMOLED-1.75, which has no backlight, the screen dims itself.

## Rotation

Turns the whole display, touch included, in 90 degree steps.  It helps when the display ends up sitting at an angle in the mask, and some housings turn the board a quarter turn, which this puts right (each board's build guide says which).  The new rotation is applied about a second after you stop scrolling the roller.

## Startup

What's shown when the display turns on: any image (see [Images](images.md)), the blank page, [KITT](kitt-and-voice-ring.md), the [Level](level.md) or the [Terminal](terminal.md), depending on what the board has.

## Phone Remote

*Boards with WiFi*

Runs the display from a phone, without touching it.  See [Phone Remote](phone-remote.md).
