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

/* Called with each block of microphone samples (mono, signed 16-bit). It runs
 * on the board's audio task, not the UI loop, so it has to be quick and must
 * not call LVGL. */
typedef void (*rcat_hal_audio_block_cb_t)(const int16_t *samples, size_t count, void *user_data);

/* Start capturing from the microphone and passing blocks of samples to cb.
 * Returns false if the board has no microphone. */
bool rcat_hal_audio_in_start(uint32_t sample_rate, rcat_hal_audio_block_cb_t cb, void *user_data);

/* Called by the board's speaker task whenever it needs more sound. Fill
 * frames with up to max_frames frames of interleaved stereo (left, right),
 * signed 16-bit, and return how many were written. *sample_rate holds the
 * rate of the last block; set it if this block is at a different rate. Return
 * 0 when there's nothing to play, and the board quietens the speaker. Like
 * the microphone callback, it runs on a board task, not the UI loop, so it
 * must not call LVGL. It can take a few milliseconds, to read and decode
 * files, as the board buffers around a tenth of a second. */
typedef size_t (*rcat_hal_audio_fill_cb_t)(int16_t *frames, size_t max_frames, uint32_t *sample_rate,
                                           void *user_data);

/* Start the speaker, calling cb for sound to play. Returns false if the board
 * has no speaker. */
bool rcat_hal_audio_out_start(rcat_hal_audio_fill_cb_t cb, void *user_data);

/* Join a WiFi network, and keep rejoining it if the connection drops, until
 * rcat_hal_wifi_disconnect(). Once connected, the core talks to the network
 * with ordinary BSD sockets. Returns false if the board has no WiFi. */
bool rcat_hal_wifi_connect(const char *ssid, const char *password);
void rcat_hal_wifi_disconnect(void);

/* The board's IP address as text ("192.168.1.23"). Returns false while it
 * isn't connected. */
bool rcat_hal_wifi_address(char *ip, size_t len);

/* Mount removable storage (the SD card). On success, *path is set to where
 * it's mounted, e.g. "/sdcard", for use with fopen() and friends. Returns
 * false if the board has no card slot or no readable card. */
bool rcat_hal_storage_mount(const char **path);

void rcat_hal_delay_ms(uint32_t ms);

#endif
