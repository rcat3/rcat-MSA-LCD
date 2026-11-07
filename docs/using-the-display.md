# Using the Display

The display is organized into features, like Images, Voice and Settings (see [Features](features/README.md) for them all).

- **Swipe up and down** to move between features.
- **Swipe left and right** to move between the pages of a feature.  Each feature remembers which page you were on.

After each swipe, dots along the right edge show which feature you're in, and dots along the bottom show which page (or "3 / 26" when there are a lot of pages).  They fade out after a moment.

On pages with a slider or roller, start your swipe beside it rather than on it, or you'll move the control instead of the page.

Every page has a dark theme, with pink controls.

Settings are saved about two seconds after you change them, and are restored when the display is turned back on.  They also survive flashing new firmware.  What's shown when the display turns on is picked in [Settings](features/settings.md#startup).

On boards with WiFi, the whole display can also be run from a phone, with the [Phone Remote](features/phone-remote.md).

## Turning it on and off

- **RP2040 and RP2350 1.28" boards:** with the slide switch in the [1.28" housing](hardware/waveshare_touch_lcd_1_28.md), which disconnects the battery.  The boards have no power button.
- **ESP32-S3-Touch-LCD-1.46:** press `PWR` on the side to turn it on.  Hold it for 2 seconds to turn it off.
- **ESP32-S3-Touch-AMOLED-1.75:** the same, with its `PWR` button.
