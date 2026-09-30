#ifndef RCAT_SETTINGS_H
#define RCAT_SETTINGS_H

#include <stdint.h>

#define SETTINGS_DEFAULT_BRIGHTNESS 50
#define SETTINGS_NAME_LEN           24

/* Startup image names that select the blank (black) tile and the KITT tile. */
#define SETTINGS_STARTUP_BLANK      "blank"
#define SETTINGS_STARTUP_KITT       "kitt"

/* Only ever add new fields to the end. Settings saved by older firmware are
 * still loaded, and any fields they don't have get their defaults. */
typedef struct
{
    uint8_t brightness;                     // backlight, percent
    uint8_t rotation;                       // display rotation, clockwise quarter turns
    char startup_image[SETTINGS_NAME_LEN];  // image name; empty means the first image
} settings_t;

/* Load the saved settings, or the defaults if nothing valid has been saved. */
void settings_init(void);

const settings_t *settings_get(void);

/* Change a setting. It's written to storage a couple of seconds after the
 * last change, so dragging a slider doesn't wear out the flash. Requires LVGL
 * to be initialised. */
void settings_set_brightness(uint8_t percent);
void settings_set_rotation(uint8_t quarter_turns);
void settings_set_startup_image(const char *name);

#endif
