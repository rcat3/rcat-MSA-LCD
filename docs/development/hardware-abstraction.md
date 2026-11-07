# The Hardware Abstraction Layer

Code in `core/` never includes anything from pico-sdk or ESP-IDF.  It reaches the hardware through the functions in [`core/include/hal.h`](../../core/include/hal.h), which every board implements.  The functions use a `rcat_hal_` prefix, because plain `hal_` names clash with vendor libraries.

| Area | Functions |
|---|---|
| Startup, display and touch | `rcat_hal_init()`, `rcat_hal_lvgl_register()`, `rcat_hal_display_set_rotation()`, `rcat_hal_backlight_set()` |
| Power and sensors | `rcat_hal_battery_voltage()`, `rcat_hal_accel_read()` |
| Settings storage | `rcat_hal_settings_read()`, `rcat_hal_settings_write()` |
| Audio | `rcat_hal_audio_in_start()` (microphone), `rcat_hal_audio_out_start()` (speaker) |
| WiFi | `rcat_hal_wifi_connect()`, `rcat_hal_wifi_disconnect()`, `rcat_hal_wifi_address()`, `rcat_hal_hotspot_start()`, `rcat_hal_hotspot_stop()`, `rcat_hal_hotspot_clients()`, `rcat_hal_random()` |
| SD card | `rcat_hal_storage_mount()` |
| Bluetooth LE | `rcat_hal_ble_scan_start()`, `rcat_hal_ble_connect()`, `rcat_hal_ble_send()`, `rcat_hal_ble_receive()` and the rest |

Display and touch reach the application through LVGL: the board registers its LVGL display and input devices in `rcat_hal_lvgl_register()`.

A board that doesn't have something returns `false` (or does nothing), and the feature that needs it hides itself.  The RP2040 and RP2350 boards, for example, return `false` from the microphone, speaker, WiFi and Bluetooth functions.

## Where the implementations are

- `platforms/pico/boards/touch_lcd_1_28/hal_board.c` - The RP2040 and RP2350 boards.
- `platforms/esp-idf/boards/<board>/hal_board.c` - Each ESP32-S3 board's own hardware: its display, touch, audio, power and pins.
- `platforms/esp-idf/components/rcat_esp32/` - What's the same on every ESP32 board: WiFi and the hotspot, Bluetooth LE, settings in NVS, mounting the SD card, the QMI8658 accelerometer, and LVGL's memory.  Its `rcat_esp32.h` has helpers the boards call, like `rcat_esp32_sd_mount()` with the board's pins.

## Adding a function

1. Declare it in `hal.h`, with a comment saying what it does and what it returns when the board can't do it.
2. Implement it in every board's `hal_board.c`, under `platforms/esp-idf/boards/` and `platforms/pico/boards/`, or once for all the ESP32 boards in `platforms/esp-idf/components/rcat_esp32/` if it doesn't depend on the board.  A board without the hardware just returns `false`.
3. Build every board (see [Checking a change](README.md#checking-a-change)).

Microphone, speaker and Bluetooth callbacks run on the board's own tasks, not the UI loop, so they mustn't call LVGL.  Each callback's comment in `hal.h` says where it runs.
