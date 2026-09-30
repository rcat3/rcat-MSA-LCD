/*
 * Hardware abstraction layer.
 *
 * Everything in core/ talks to the hardware through these functions only.
 * Each board under platforms/<sdk>/boards/ provides an implementation.
 * Display and touch are exposed to the core through LVGL, so the board is
 * responsible for registering its LVGL display and input drivers.
 *
 * The functions use a project-specific prefix because plain "hal_" names
 * clash with vendor libraries (ESP-IDF's WiFi library has a hal_init()).
 */
#ifndef RCAT_HAL_H
#define RCAT_HAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Bring up clocks, buses, the LCD panel and the touch controller.
 * Returns 0 on success. */
int rcat_hal_init(void);

/* Register the display and input devices with LVGL and start the LVGL tick.
 * Called once, after lv_init(). */
void rcat_hal_lvgl_register(void);

/* Rotate the whole display, touch input included, clockwise in 90 degree
 * steps (0 = normal, 1 = 90, 2 = 180, 3 = 270 degrees). */
void rcat_hal_display_set_rotation(uint8_t quarter_turns);

/* Set the backlight brightness in percent (0-100). */
void rcat_hal_backlight_set(uint8_t percent);

/* Read the battery voltage in volts.
 * Returns false if the board can't measure it. */
bool rcat_hal_battery_voltage(float *volts);

/* Persistent storage for one small block of settings (up to 256 bytes).
 * The caller validates the contents, so the board just stores raw bytes.
 * rcat_hal_settings_read returns false if nothing could be read (no storage,
 * or nothing saved yet). */
bool rcat_hal_settings_read(void *data, size_t len);
bool rcat_hal_settings_write(const void *data, size_t len);

void rcat_hal_delay_ms(uint32_t ms);

#endif
