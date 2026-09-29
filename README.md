# Rubbercat's MSA Millennium LCD Screen Software (rcat-MSA-LCD)

## Hardware

This project now supports both the [Waveshare RP2040-Touch-LCD-1.28 LCD Display](https://www.amazon.com/dp/B0C4LRRVVN) as well as the [Waveshare RP2350-Touch-LCD-1.28 LCD Display](https://www.amazon.com/dp/B0DLBF5QKK).  The RP2040 version has 264kB of SRAM while the RP2350 version has 520kB.  They're about the same price, so I'd recommend just getting the RP2350 version.  This is especially true if you're interested in displaying animated GIFs.  GIFs are very memory intensive.  The RP2040 will not be able to show an animated GIF that fills the screen.

### 3D Print Models

The 3D model files can be found at [this link](https://www.printables.com/model/538771-rp2040-lcd-128-msa).  I don't think I used the pieces as the original author intended.  Therefore, not all of the pieces were used.  I recommend printing the pieces in `light v23.stl` and `vpu_threaded.3mf`.  I ended up drilling a 1/4" hole through the threaded piece so that I could mount a switch to conveniently disconnect battery power.

### Bill of Materials

Other things you'll need:
- [Waveshare RP2040-Touch-LCD-1.28 LCD Display](https://www.amazon.com/dp/B0C4LRRVVN) *OR* [Waveshare RP2350-Touch-LCD-1.28 LCD Display](https://www.amazon.com/dp/B0DLBF5QKK)
- [3.7V 310mAh 502030 Lithium Polymer Battery](https://www.amazon.com/dp/B08TBY5BFL) : It's more common to find this battery size in 250mAh.  You can get a 250mAh battery if this one is not available.  **Note: If you get the battery at this link, the connector polarity is opposite of what the RP2040 wants.  You'll need to switch the + and - wires around in the connector or solder on a new connector.  Do not connect to the RP2040 until you've done this!
- [JST 1.25mm 2 pin connector cables with male and female connectors](https://www.amazon.com/dp/B013JRWCBU)
- [WMYCONGCONG Latching Push Button Switch](https://www.amazon.com/dp/B07BMNYJ13)
- M2 heat set inserts
- M2x16mm screws


## Software

I created this project because I was having trouble with all of the other existing projects intended for this hardware.  They all failed to initialize the touch screen over I2C or were otherwise unreliable on my hardware.  The Waveshare example projects seemed to work fine, though.  Therefore, I just hacked this together from the [Waveshare example project that uses the LVGL GUI library](https://www.waveshare.com/wiki/RP2040-Touch-LCD-1.28#LVGL_Example_Demo).  The LVGL library opens up a lot of cool possibilities for effects if you want to spend the time working with it.  Take a look at the [LVGL documentation](https://docs.lvgl.io/8.1/) to get an idea of what it's capable of.  The original demo can be downloaded from [this link](https://files.waveshare.com/upload/1/16/RP2040-Touch-LCD-1.28-LVGL.zip).

My implementation originally focused on displaying static images, but it now supports small animated GIFs as well.  You can swipe up/down to change images.  The last few tiles in the sequence allow you to adjust the LCD brightness, view the battery voltage, and rotate the images.  Image rotation might be helpful if you're having trouble orienting the LCD display properly in the MSA's VPU threads.

If you want to change the images, you'll need to rebuild the software image and reflash the device.

### Building the software: Installing pico-sdk

First, you're going to need to clone the [pico-sdk](https://github.com/raspberrypi/pico-sdk) project from GitHub.  Make a new folder somewhere and `cd` into that folder.  Then `git clone https://github.com/raspberrypi/pico-sdk.git`.  Note this folder location.  You'll need it in the next step.

### Getting the code

LVGL is included as a git submodule, so clone with `--recursive`:

```
git clone --recursive https://github.com/rcat3/rcat-MSA-LCD.git
```

If you already cloned without it, run `git submodule update --init` inside the repo.

### Building the rcat-MSA-LCD project

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

### Flashing the image

Connect the USB-C port of the RP2040 to your computer.  To flash a new image onto the hardware, press and hold the `BOOT` button.  Press and release `RESET`.  Now you can release `BOOT`.  The screen should be off.  A mass storage device should appear on the computer.  Copy the .uf2 file to this mass storage device.  When copying is complete, the RP2040 should reset and begin running the new image.



## Modifying the code to add or change images

### Converting the image

All images must be converted to C arrays that get incorporated into the source code.  Use the [LVGL Image Converter](https://lvgl.io/tools/imageconverter) to convert the images.  The image dimensions should be 240x240 or less.

#### For Static Images

1. In the image converter, select *LVGL v8*.
2. Click *Select image file(s)* and navigate to your desired image.
3. For *Color Format*, select *CF_TRUE_COLOR*.
4. Output format should be *C array*.
5. Leave the boxes unchecked.  
6. Click *Convert*.  The converted image will download automatically.
7. Copy the downloaded .c file to `core/media/images`.

#### For Animated GIFs

The hardware is limited in terms of available memory.  You need to be mindful of how large of an animated GIF you're trying to load.  If you exceed the available memory, the application will likely crash on start-up.  The biggest factors in predicting whether an animated GIF will work are it's resolution and color depth.  Per the [LVGL documentation regarding GIF decoding](https://docs.lvgl.io/8.3/libs/gif.html), memory requirements are:

- 8 bit color depth: 3 x image width x image height
- 16 bit color depth: 4 x image width x image height
- 32 bit color depth: 5 x image width x image height

For the RP2040 hardware, here are some rough (untested) guidelines based on the above requirements:
- up to ~60kB: Probably okay.
- ~60kB-100kB: Starting to get questionable.
- over 134kB: Risky / Likely to crash depending on rest of app.

Needless to say, you won't be able to fill the 240x240 screen.  If you flash a new image and it doesn't start running as expected, you've likely encountered a memory allocation error.  Reduce the size of your animated GIF and try again.  The limits are obviously also
dependent on how many other pictures you're trying to compile in.

I haven't extensively tested the RP2350 hardware yet, but you're probably good up to about 384kB.

To [convert an animated GIF](https://lvgl.io/tools/imageconverter) for use in the program:
1. In the image converter, select *LVGL v8*.
2. Click *Select image file(s)* and navigate to your desired image.
3. For *Color Format*, select *CF_RAW*.
4. Output format should be *C array*.
5. Leave the boxes unchecked.  
6. Click *Convert*.  The converted image will download automatically.
7. Copy the downloaded .c file to `core/media/images`.

### Modifying the source code

The list of images lives in [`core/media/image_list.c`](core/media/image_list.c).  Images are shown in the order they're listed.  To add an image, declare it and add it to the list.  Substitute `your_image` for your image name (it's the same as the name of the downloaded .c file).  The second value says whether the image is an animated GIF: `false` for a static image, `true` for an animated GIF.

```
LV_IMG_DECLARE(your_image);

const media_image_t media_images[] = {
    ...
    { &your_image, false },
};
```

To remove an image, just take it out of the list.  Images in `core/media/images` that aren't in the list don't take up any space in the firmware.

## Project layout

- `core/` - The application itself (UI, image list, settings).  This code doesn't depend on any particular board.
- `core/include/hal.h` - The small set of functions each board has to provide (display, touch, backlight, battery).
- `platforms/pico/` - Build files for the Raspberry Pi RP2040/RP2350 boards.  `boards/*.cmake` lists the supported boards, and `boards/touch_lcd_1_28/` has the Waveshare drivers for the 1.28" round display.
- `third_party/lvgl/` - The [LVGL](https://lvgl.io) graphics library (git submodule).
- `imgs/` - Source images.

The idea is to make it straightforward to add other displays and microcontrollers (ESP32-S3 is next) without copying the application code around.
