/*
 * Helpers shared by the ESP32 boards' HALs. The shared parts of the HAL
 * itself (WiFi, the hotspot, settings, Bluetooth LE and LVGL's memory) are
 * implemented in this component too, so a board only has to provide its own
 * hardware.
 */
#ifndef RCAT_ESP32_H
#define RCAT_ESP32_H

#include <stdbool.h>

#include "driver/i2c_master.h"

/* Get NVS ready for rcat_hal_settings_read/write. Call it early in
 * rcat_hal_init(). */
void rcat_esp32_settings_init(void);

/* Mount the SD card in 1-bit SD mode on these pins, at "/sdcard", the first
 * time it's called. For the board's rcat_hal_storage_mount(). */
bool rcat_esp32_sd_mount(int clk, int cmd, int d0, const char **path);

/* Read the QMI8658's accelerometer on the bus, in g, in the IMU's own axes.
 * It's set up the first time. Returns false if it doesn't answer. */
bool rcat_esp32_qmi8658_read(i2c_master_bus_handle_t bus, float a[3]);

#endif
