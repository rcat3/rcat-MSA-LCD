# Rubbercat's MSA Millennium LCD Screen Software (rcat-MSA-LCD)

Firmware for small round Waveshare touch displays, originally written for a display mounted in the front of an MSA Millennium gas mask.  It shows a sequence of static images and animated GIFs that you swipe through, plus a few settings screens.

## Hardware

Supported boards:

| Board | Screen | Notes |
|---|---|---|
| [Waveshare RP2040-Touch-LCD-1.28](https://www.amazon.com/dp/B0C4LRRVVN) | 240x240 round | 264kB of RAM.  Fine for static images, limited for animated GIFs. |
| [Waveshare RP2350-Touch-LCD-1.28](https://www.amazon.com/dp/B0DLBF5QKK) | 240x240 round | 520kB of RAM.  Same price as the RP2040 version, so I'd recommend this one. |
| [Waveshare ESP32-S3-Touch-LCD-1.46](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.46) | 412x412 round | 8MB of PSRAM, microphone, speaker, TF card slot, WiFi and Bluetooth.  See [the ESP32-S3 notes](#esp32-s3-touch-lcd-146-notes) below. |

The two 1.28" boards are the ones used in the MSA mask build described next.

### MSA Mask Build (1.28" boards)

#### 3D Print Models

The 3D model files can be found at [this link](https://www.printables.com/model/538771-rp2040-lcd-128-msa).  I don't think I used the pieces as the original author intended.  Therefore, not all of the pieces were used.  I recommend printing the pieces in `light v23.stl` and `vpu_threaded.3mf`.  I ended up drilling a 1/4" hole through the threaded piece so that I could mount a switch to conveniently disconnect battery power.

#### Bill of Materials

Other things you'll need:
- [Waveshare RP2040-Touch-LCD-1.28 LCD Display](https://www.amazon.com/dp/B0C4LRRVVN) *OR* [Waveshare RP2350-Touch-LCD-1.28 LCD Display](https://www.amazon.com/dp/B0DLBF5QKK)
- [3.7V 310mAh 502030 Lithium Polymer Battery](https://www.amazon.com/dp/B08TBY5BFL) : It's more common to find this battery size in 250mAh.  You can get a 250mAh battery if this one is not available.  **Note: If you get the battery at this link, the connector polarity is opposite of what the RP2040 wants.  You'll need to switch the + and - wires around in the connector or solder on a new connector.  Do not connect to the RP2040 until you've done this!**
- [JST 1.25mm 2 pin connector cables with male and female connectors](https://www.amazon.com/dp/B013JRWCBU)
- [WMYCONGCONG Latching Push Button Switch](https://www.amazon.com/dp/B07BMNYJ13)
- M2 heat set inserts
- M2x16mm screws

## Using the Display

The display is organized into features.  **Swipe up and down** to move between features, and **swipe left and right** to move between the pages of a feature.  Each feature remembers which page you were on.  After each swipe, dots along the right edge show which feature you're in and dots along the bottom show which page (or "3 / 26" when there are a lot of pages).  They fade out after a moment.

| Feature | Pages (left to right) |
|---|---|
| **Images** | Each image (images from the SD card first), then a blank (black) page. |
| **Voice** (boards with a microphone) | **KITT**: three columns of red bars that light up with your voice, like KITT's voice box in Knight Rider.  **Voice Ring**: turns on a ring around the edge of the screen that glows with your voice while the Images feature is showing, and picks its color. |
| **Settings** | **Brightness**: the backlight brightness, plus the battery voltage.  **Rotation**: rotates the whole display in 90 degree steps, which helps if the display ends up sitting at an angle in the mask's threads.  The new rotation is applied about a second after you stop scrolling the roller.  **Startup**: picks what's shown when the display turns on (any image, KITT or the blank page). |

On pages with a slider or roller, start your swipe beside it rather than on it, or you'll move the control instead of the page.

Settings are saved about two seconds after you change them and are restored when the display is powered back on.  They also survive flashing new firmware.

The KITT bars and the voice ring adjust to the surroundings on their own.  A quiet room stays dark, normal talking uses most of the range, and they scale back for loud talking or a noisy room (it takes a few seconds to adjust).

### ESP32-S3-Touch-LCD-1.46 Notes

- **Power:** The board runs from USB-C or from a 3.7V LiPo battery on its 2-pin MX1.25 connector (check the polarity before plugging a battery in).  On battery, press the `PWR` button on the side to turn it on.  The firmware keeps the power switched on after you let go.  Hold `PWR` for 2 seconds to turn it off.
- **Images** are converted for the 412x412 screen automatically when you build, so the same image files work on every board (see [Adding or Changing Images](#adding-or-changing-images)).
- **Rotation:** The display controller can't rotate the picture by 90 degrees itself, so the ESP32 rotates it while drawing.  It's fast enough that you shouldn't notice.
- **Microphone:** used by the Voice feature (KITT and the voice ring).
- **SD card:** images on the card are shown along with the built-in ones.  See [Images on the SD Card](#images-on-the-sd-card-esp32-s3-board).
- **Not used yet:** the speaker, IMU, clock chip, WiFi and Bluetooth.
- There isn't a 3D printed case for this board in the mask build yet.

## Software

I created this project because I was having trouble with all of the other existing projects intended for this hardware.  They all failed to initialize the touch screen over I2C or were otherwise unreliable on my hardware.  The Waveshare example projects seemed to work fine, though.  Therefore, I started from the [Waveshare example project that uses the LVGL GUI library](https://www.waveshare.com/wiki/RP2040-Touch-LCD-1.28#LVGL_Example_Demo), and it has since grown to support more boards.  The LVGL library opens up a lot of cool possibilities for effects if you want to spend the time working with it.  Take a look at the [LVGL documentation](https://docs.lvgl.io/) to get an idea of what it's capable of (this project uses LVGL v9.6).

If you want to change the images, you'll need to rebuild the firmware and reflash the device.

### Getting the code

LVGL is included as a git submodule, so clone with `--recursive`:

```
git clone --recursive https://github.com/rcat3/rcat-MSA-LCD.git
```

If you already cloned without it, run `git submodule update --init` inside the repo.

Building needs Python 3 with [Pillow](https://pypi.org/project/pillow/) installed (`pip install pillow`), which is used to convert the images.

### Building for the RP2040/RP2350 boards

#### Installing pico-sdk

First, you're going to need to clone the [pico-sdk](https://github.com/raspberrypi/pico-sdk) project from GitHub.  Make a new folder somewhere and `cd` into that folder.  Then `git clone https://github.com/raspberrypi/pico-sdk.git`.  Note this folder location.  You'll need it in the next step.

#### Building the firmware

```
cd rcat-MSA-LCD   (wherever you cloned this repo)
export PICO_SDK_PATH=$HOME/pico-sdk   (or wherever you put it)
```

Use an absolute path for `PICO_SDK_PATH`.  A relative path gets resolved from inside the `build` folder, not from where you run the command, which is confusing.

If you're building for the RP2350 version of the display:
```
cmake -S platforms/pico -B build -DBOARD=waveshare_rp2350_touch_lcd_1_28
```

Otherwise, if you're building for the RP2040 display:
```
cmake -S platforms/pico -B build -DBOARD=waveshare_rp2040_touch_lcd_1_28
```

Then for either version, run:
```
cmake --build build -j
```

When compilation is complete, you should have a file called `build/rcat-msa-lcd.uf2`.  This is the image file you'll flash in the next step.

If you switch between boards, delete the `build` folder first (or use a different build folder for each board).

#### Flashing

Connect the USB-C port of the RP2040 to your computer.  To flash a new image onto the hardware, press and hold the `BOOT` button.  Press and release `RESET`.  Now you can release `BOOT`.  The screen should be off.  A mass storage device should appear on the computer.  Copy the .uf2 file to this mass storage device.  When copying is complete, the RP2040 should reset and begin running the new image.

### Building for the ESP32-S3 board

#### Installing ESP-IDF

The ESP32 boards are built with Espressif's [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/get-started/linux-macos-setup.html), version 5.5.  On Linux, the short version is:

```
mkdir -p ~/esp && cd ~/esp
git clone -b v5.5.5 --recursive https://github.com/espressif/esp-idf.git esp-idf-v5.5.5
cd esp-idf-v5.5.5
./install.sh esp32s3
```

In each new terminal where you want to build, set up the environment first:

```
. ~/esp/esp-idf-v5.5.5/export.sh
```

ESP-IDF uses its own copy of Python, so Pillow needs to be installed there too.  This only needs to be done once, in a terminal where you've run `export.sh`:

```
python -m pip install pillow
```

#### Building the firmware

From the top of the repo:

```
idf.py -C platforms/esp-idf -B build-s3-146 -DBOARD=waveshare_esp32s3_touch_lcd_1_46 build
```

The first build downloads the display and touch drivers from Espressif's component registry.

#### Flashing

Connect the board's USB-C port and run:

```
idf.py -C platforms/esp-idf -B build-s3-146 -p /dev/ttyACM0 flash monitor
```

Use the port your board shows up as (on Linux it's usually `/dev/ttyACM0`).  `monitor` shows the board's log output.  Press `Ctrl+]` to exit it.

A few things that can come up:

- **The log stops at `waiting for download`.**  The board was reset into its programming mode instead of starting the firmware.  Press `Ctrl+T` then `Ctrl+R` in the monitor to reset it, or unplug it and plug it back in.
- **The board isn't found when flashing.**  Hold the `BOOT` button while plugging in the USB cable, then try again.
- **Watching the log without flashing:** `idf.py -C platforms/esp-idf -B build-s3-146 -p /dev/ttyACM0 monitor`
- **After updating the code, a setting doesn't seem to take effect.**  ESP-IDF keeps its configuration in the build folder and doesn't apply new defaults to an existing one.  Delete the `build-s3-146` folder and build again.

## Adding or Changing Images

1. Put the image in the `imgs/` folder.  PNG, BMP, JPG and animated GIF files all work.
2. Add it to the list in [`core/media/image_list.c`](core/media/image_list.c).  Images are shown in the order they're listed.  The name is the file name without the extension, so `imgs/your_image.png` is `your_image`.  The second value says whether the image is an animated GIF: `false` for a static image, `true` for an animated GIF.

   ```
   LV_IMAGE_DECLARE(your_image);

   const builtin_image_t builtin_images[] = {
       ...
       IMAGE(your_image, false),
   };
   ```

3. Rebuild and flash.  The build converts the images automatically.

The image name is also what shows up in the *Startup* setting.  To remove an image, just take it out of the list.  Images in `imgs/` that aren't in the list don't take up any space in the firmware.

### Images on the SD Card (ESP32-S3 board)

The ESP32-S3 board can also show images from its TF (micro SD) card, so you can change them without rebuilding the firmware.

1. Format the card as FAT32 (most cards come that way) and make a folder called `images` on it.  If there's no `images` folder, the top level of the card is used instead.
2. Copy PNG, JPG, BMP or animated GIF files into it.
3. Put the card in the board and switch it on.  It shows *Loading images...* while it reads the card.

The images from the card come first, sorted by file name, followed by the built-in images.  They can be picked in the *Startup* setting by their file name (without the extension).  If a card image has the same name as a built-in image, the card image is the one picked.  The card is only read when the board starts, so restart it after changing the card.

Images are sized for the screen the same way as the built-in images (see below).  Static images are resized once, when they're loaded.  Animated GIFs are scaled while they play, which costs a little time on every frame and looks slightly softer.  For the best results, prepare images on your computer first with `tools/prepare_sd.py`.  It resizes GIFs and static images ahead of time and marks them as prepared, so the board shows them as they are:

```
python3 tools/prepare_sd.py my_images/*.gif my_images/*.png --size 412 --out /media/you/SDCARD/images
```

Use the path where your card is mounted.  `--size 412` is the screen size of the ESP32-S3 board.

Limits:
- Up to 32 images are loaded from the card.
- Images can be up to 1024x1024 pixels.
- BMP files must be 16, 24 or 32-bit (not 1, 4 or 8-bit).  Files that can't be read are skipped, and the reason is printed in the monitor log.
- File names can be long, but only the first 23 characters (without the extension) are used for the *Startup* setting.
- Hidden files (like the `._` files macOS leaves on cards) are ignored.

### Image sizes

Images are drawn for a 240x240 screen.  On a bigger screen they're scaled up by the same amount, so a 240x240 image fills the 412x412 screen of the ESP32-S3 board, and a 120x120 image fills the middle half of either screen.  If an image is bigger than 240x240, it's treated as full screen artwork and scaled to fit each screen, so higher resolution artwork will look sharper on the bigger screen.

Animated GIFs that need resizing are re-encoded, without transparent pixels and with a black background, so a resized GIF can come out bigger than the original file.  GIFs that are already the right size are used as they are.

The conversion is done by `tools/img2c.py`.  You don't normally need to run it yourself, but you can (for example `python3 tools/img2c.py imgs/home.bmp --size 412 --out home.c`) to see what it produces.

### Animated GIF memory use

Animated GIFs are decoded in RAM while they're on screen, so on the RP2040/RP2350 boards you need to be mindful of how large of an animated GIF you're trying to load.  If you exceed the available memory, the application will likely crash on start-up.  Each GIF needs about:

- 2 bytes x image width x image height, plus
- about 24kB for the decoder

For example, a 200x168 GIF needs about 90kB and a full screen 240x240 GIF needs about 140kB.  The RP2040 has 264kB of RAM in total and the RP2350 has 520kB, and the rest of the app needs some of that too.  If you flash a new image and it doesn't start running as expected, you've likely run out of memory.  Reduce the size of your animated GIF and try again.

The ESP32-S3 board decodes GIFs into its 8MB of PSRAM, so even full screen GIFs fit easily.

## Project layout

- `core/` - The application itself (UI, image list, settings).  This code doesn't depend on any particular board.
- `core/include/hal.h` - The small set of functions each board has to provide (display, touch, backlight, battery, settings storage).
- `platforms/pico/` - Build files for the Raspberry Pi RP2040/RP2350 boards.  `boards/*.cmake` lists the supported boards, and `boards/touch_lcd_1_28/` has the Waveshare drivers for the 1.28" round display.
- `platforms/esp-idf/` - Build files for the ESP32 boards.  `boards/` has one folder per board with its drivers, pin assignments and settings.
- `third_party/lvgl/` - The [LVGL](https://lvgl.io) graphics library (git submodule).
- `tools/img2c.py` - Converts images and GIFs into C files.  The build runs it for every image in `imgs/`.
- `tools/prepare_sd.py` - Resizes and re-encodes images for an SD card.
- `tools/gen_lv_conf.py` - Generates the platforms' `lv_conf.h` files (LVGL settings) from the LVGL template.  Run it after updating the LVGL submodule.
- `imgs/` - The images.

Adding another display means adding a board folder with its HAL (`hal_board.c`) and its screen size, without copying the application code around.
