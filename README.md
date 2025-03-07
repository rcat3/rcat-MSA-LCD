# Rubbercat's MSA Millennium LCD Screen Software (rcat-MSA-LCD)

## Hardware

### 3D Print Models

The 3D model files can be found at [this link](https://www.printables.com/model/538771-rp2040-lcd-128-msa).  I don't think I used the pieces as the original author intended.  Therefore, not all of the pieces were used.  I recommend printing the pieces in `light v23.stl` and `vpu_threaded.3mf`.  I ended up drilling a 1/4" hole through the threaded piece so that I could mount a switch to conveniently disconnect battery power.

### Bill of Materials

Other things you'll need:
- [Waveshare RP2040-Touch-LCD-1.28 LCD Display](https://www.amazon.com/dp/B0C4LRRVVN)
- [3.7V 310mAh 502030 Lithium Polymer Battery](https://www.amazon.com/dp/B08TBY5BFL) : It's more common to find this battery size in 250mAh.  You can get a 250mAh battery if this one is not available.  **Note: If you get the battery at this link, the connector polarity is opposite of what the RP2040 wants.  You'll need to switch the + and - wires around in the connector or solder on a new connector.  Do not connect to the RP2040 until you've done this!
- [JST 1.25mm 2 pin connector cables with male and female connectors](https://www.amazon.com/dp/B013JRWCBU)
- [WMYCONGCONG Latching Push Button Switch](https://www.amazon.com/dp/B07BMNYJ13)
- M2 heat set inserts
- M2x16mm screws


## Software

I created this project because I was having trouble with all of the other existing projects intended for this hardware.  They all failed to initialize the touch screen over I2C or were otherwise unreliable on my hardware.  The Waveshare example projects seemed to work fine, though.  Therefore, I just hacked this together from the [Waveshare example project that uses the LVGL GUI library](https://www.waveshare.com/wiki/RP2040-Touch-LCD-1.28#LVGL_Example_Demo).  The LVGL library opens up a lot of cool possibilities for effects if you want to spend the time working with it.  Take a look at the [LVGL documentation](https://docs.lvgl.io/8.1/) to get an idea of what it's capable of.  The original demo can be downloaded from [this link](https://files.waveshare.com/upload/1/16/RP2040-Touch-LCD-1.28-LVGL.zip).

My implementation focuses on displaying static images.  You can swipe up/down to change images.  The last few tiles in the sequence allow you to adjust the LCD brightness, view the battery voltage, and rotate the images.  Image rotation might be helpful if you're having trouble orienting the LCD display properly in the MSA's VPU threads.

If you want to change the images, you'll need to rebuild the software image and reflash the device.

### Building the software: Installing pico-sdk

First, you're going to need to clone the [pico-sdk](https://github.com/raspberrypi/pico-sdk) project from GitHub.  Make a new folder somewhere and `cd` into that folder.  Then `git clone https://github.com/raspberrypi/pico-sdk.git`.  Note this folder location.  You'll need it in the next step.

### Building the rcat-MSA-LCD project

```
cd rcat-MSA-LCD   (wherever you cloned this repo)
mkdir build
cd build
export PICO_SDK_PATH=../../pico-sdk   (or wherever you put it)
cmake ..
make
```

When compilation is complete, you should have a file called `rcat-MSA-LCD.uf2`.  This is the image file you'll flash in the next step.

### Flashing the image

Connect the USB-C port of the RP2040 to your computer.  To flash a new image onto the hardware, press and hold the `BOOT` button.  Press and release `RESET`.  Now you can release `BOOT`.  The screen should be off.  A mass storage device should appear on the computer.  Copy the .uf2 file to this mass storage device.  When copying is complete, the RP2040 should reset and begin running the new image.



## Modifying the code to add or change images

### Converting the image

All images must be converted to C arrays that get incorporated into the source code.  Use the [LVGL Image Converter](https://lvgl.io/tools/imageconverter) to convert the images.  The image dimensions should be 240x240.

1. In the image converter, select *LVGL v8*.
2. Click *Select image file(s)* and navigate to your desired image.
3. For *Color Format*, select *CF_TRUE_COLOR*.
4. Output format should be *C array*.
5. Leave the boxes unchecked.  
6. Click *Convert*.  The converted image will download automatically.
7. Copy the downloaded .c file to `examples/src`.

### Modifying the source code

1. Edit `LVGL_example.c` to add your image (or replace an existing image).
2. Find the `Widgets_Init()` function around [line 144](https://github.com/rcat3/rcat-MSA-LCD/blob/master/examples/src/LVGL_example.c#L144).
3. If you're adding an image, increment `num_imgs` as necessary.  If you're replacing an image, you can leave it as is.  If you're removing an image, decrement `num_imgs`.
4. Add calls for your image.  You should add the following two lines to their respective groups.  Substitute `your_image` for your image name.  Pick an appropriate number for the third parameter in `add_pic_tile()`.  Each call should have it's own unique number (in order).
```
LV_IMG_DECLARE(your_image);
add_pic_tile(tv, &your_image, 5);
```
