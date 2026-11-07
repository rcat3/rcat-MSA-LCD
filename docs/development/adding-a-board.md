# Adding a Board

A new display means a board folder with its HAL and its screen size, without copying the application around.  Use the closest existing board as the starting point.

## In the firmware

### An RP2040 or RP2350 board

1. Add `platforms/pico/boards/<board>.cmake`, like [`waveshare_rp2350_touch_lcd_1_28.cmake`](../../platforms/pico/boards/waveshare_rp2350_touch_lcd_1_28.cmake).  It sets `PICO_BOARD`, `BOARD_DIR` (the folder with its HAL), `RCAT_IMAGE_SIZE` (the screen's size in pixels) and `RCAT_LARGE_IMAGES` (whether full screen animated GIFs fit in its RAM).
2. If its hardware differs from the 1.28" boards, add a folder for its HAL and drivers under `platforms/pico/boards/`, and point `BOARD_DIR` at it.
3. Build it with `-DBOARD=<board>`.

### An ESP32 board

Add `platforms/esp-idf/boards/<board>/`, named after the board, with:

- `board.cmake` - Sets `IDF_TARGET`, `RCAT_IMAGE_SIZE` and `RCAT_LARGE_IMAGES`.
- `CMakeLists.txt` - Registers the board's component, which requires `rcat_core` and `rcat_esp32`.
- `hal_board.c` - The board's own HAL: display, touch, audio, battery, accelerometer axes and the SD card's pins.  Everything else is in `components/rcat_esp32/` already.
- `idf_component.yml` - The drivers it needs from Espressif's component registry.  The first build makes `dependencies.lock`, which pins their versions; commit it.
- `sdkconfig.defaults` - Its flash size, PSRAM and partition table, starting from another board's.
- `partitions.csv`

Build it with `-DBOARD=<board>` and its own build folder.  The [ESP32-S3-Touch-AMOLED-1.75](../../platforms/esp-idf/boards/waveshare_esp32s3_touch_amoled_1_75/) is a recent example of all of this.

### Then

- Check which features it can run, and set them in the platform's `features.cmake` if a feature needs switching off for the whole platform.
- Add it to the tables in the [top-level README](../../README.md#hardware) and in [`docs/features/README.md`](../features/README.md), and give it a build guide in [`docs/hardware/`](../hardware/).

## A housing for it

The housings are in `hardware/boards/<board>/`, named like the firmware's board folder, with the board's dimensions in `board.scad` and the mask's thread and turning collar shared from `hardware/masks/`.  [`hardware/README.md`](../../hardware/README.md#adding-a-board) has the steps.  Add a scene for it in `tools/doc_renders/` and run `render.sh`, so it has a picture in the documentation.
