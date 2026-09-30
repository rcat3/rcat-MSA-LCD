#ifndef RCAT_SETTINGS_H
#define RCAT_SETTINGS_H

#include <stdint.h>

#define SETTINGS_DEFAULT_BRIGHTNESS 50

typedef struct
{
    uint8_t brightness;     // backlight, percent
    uint8_t rotation;       // display rotation, clockwise quarter turns
} settings_t;

/* Load the saved settings, or the defaults if nothing valid has been saved. */
void settings_init(void);

const settings_t *settings_get(void);

/* Change a setting. It's written to storage a couple of seconds after the
 * last change, so dragging a slider doesn't wear out the flash. Requires LVGL
 * to be initialised. */
void settings_set_brightness(uint8_t percent);
void settings_set_rotation(uint8_t quarter_turns);

#endif
