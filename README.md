# Rubbercat's MSA Millennium LCD Screen Software (rcat-MSA-LCD)

Firmware for small round Waveshare touch displays, originally written for a display mounted in the front of an MSA Millennium gas mask.  It shows a sequence of static images and animated GIFs that you swipe through, plus a few settings screens.

## Hardware

Supported boards:

| Board | Screen | Notes |
|---|---|---|
| [Waveshare RP2040-Touch-LCD-1.28](https://www.amazon.com/dp/B0C4LRRVVN) | 240x240 round | 264kB of RAM.  Fine for static images, limited for animated GIFs. |
| [Waveshare RP2350-Touch-LCD-1.28](https://www.amazon.com/dp/B0DLBF5QKK) | 240x240 round | 520kB of RAM.  Same price as the RP2040 version, so I'd recommend this one. |
| [Waveshare ESP32-S3-Touch-LCD-1.46](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.46) | 412x412 round | 8MB of PSRAM, microphone, speaker, TF card slot, WiFi and Bluetooth.  See [the ESP32-S3 notes](#esp32-s3-touch-lcd-146-notes) below. |
| [Waveshare ESP32-S3-Touch-AMOLED-1.75](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75) | 466x466 round AMOLED | Like the 1.46", plus two microphones, a speaker amplifier and a battery charger on the board.  **New, and not yet tested on the board.**  See [its notes](#esp32-s3-touch-amoled-175-notes) below. |

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
| **Voice** (boards with a microphone) | **KITT**: three columns of red bars that light up with your voice, like KITT's voice box in Knight Rider.  **Voice Ring**: turns on a ring around the edge of the screen that glows with your voice while the Images feature is showing, and picks its color.  **Voice Effect**: changes your voice (see [Voice Effects](#voice-effects-esp32-s3-board)).  **Voice Lab** (boards with WiFi): streams the microphone to a computer, for trying out voice effects (see [Voice Lab](#voice-lab-esp32-s3-board)). |
| **Sound** (boards with a speaker) | **Player**: plays the sound files on the SD card, with previous, play/pause and next buttons.  Sound keeps playing in the background while you look at images or anything else.  **Options**: the volume, what happens at the end of a track (*Repeat all*, *Repeat one* or *Play once*), and *Play at startup*, which starts playing the last track as soon as the display turns on. |
| **Lovense** (boards with Bluetooth) | Controls up to three Lovense devices at once.  **Control**: a ring around the edge for each of the device's motors (vibration, rotation, the Max's air pump and so on), with the level on its knob, 0 to 20 for most.  Drag a knob around its ring to set the level.  Only the knobs respond, so you can swipe from anywhere else.  The device's name is in the middle, with arrows either side to step through the connected devices.  **STOP** stops every device, and any pattern, as soon as you touch it.  **Devices**: turn your devices on and they show up here while it's on screen.  Tap one to connect (it turns green), or tap it again to stop it and disconnect.  **Patterns**: tap a pattern to play it, and tap it again to stop it.  The arrows at the top pick which device it plays on, or all of them, and **-**/**+** at the bottom set the speed.  While a pattern plays, each ring on the Control page sets how high that motor goes, and a thin line inside the ring shows the level as it plays.  Six patterns are built in, and you can add your own (see [Patterns on the SD Card](#patterns-on-the-sd-card-esp32-s3-board)).  **React to**: drives the devices from your voice or how you move, on top of any pattern.  *Voice* follows how loud you talk.  *Moving* runs while you move and stops soon after you stop.  *Still* runs only once you've held still with the display upright for a second (set the display's Rotation first, so upright is right).  *Motion* gets stronger the more you move, and *Tilt* the further you tilt from upright.  Tap the one that's on to turn it off.  The line under the list shows what it's sensing.  As with patterns, the rings on the Control page set how high each motor goes.  **STOP** turns the reaction off too.  **Options**: *Motor Hold*: on, a motor keeps its level when you let go of its knob; off, it springs back to 0.  The connections stay up while you look at other features.  Everything here can also be done from a phone, with the Phone Remote in Settings (see [Phone Remote](#phone-remote-esp32-s3-board)). |
| **Terminal** (boards with WiFi) | Shows what's typed on the phone remote like an old green-screen terminal: blocky green letters that type themselves out behind a block cursor, which blinks once they're done, with faint scan lines over the screen.  The letter size is picked on the phone: *Auto* uses the biggest letters the whole message fits in (so a word or two fills the screen), down to 16 px, or pick one of 8, 16, 24, 32, 40 or 48 px.  A message too long for the size scrolls like a terminal, keeping the newest lines on screen.  It shows a `>` prompt until something's sent.  It can be picked in *Startup*. |
| **Settings** | **Brightness**: the backlight brightness, plus the battery voltage.  **Rotation**: rotates the whole display in 90 degree steps, which helps if the display ends up sitting at an angle in the mask's threads.  The new rotation is applied about a second after you stop scrolling the roller.  **Startup**: picks what's shown when the display turns on (any image, KITT, the Terminal or the blank page).  **Phone Remote** (boards with WiFi): runs the display from a phone, without touching it (see [Phone Remote](#phone-remote-esp32-s3-board)). |

Every page has a dark theme, with pink controls.

On pages with a slider or roller, start your swipe beside it rather than on it, or you'll move the control instead of the page.

Settings are saved about two seconds after you change them and are restored when the display is powered back on.  They also survive flashing new firmware.

The KITT bars and the voice ring adjust to the surroundings on their own.  A quiet room stays dark, normal talking uses most of the range, and they scale back for loud talking or a noisy room (it takes a few seconds to adjust).

### ESP32-S3-Touch-LCD-1.46 Notes

- **Power:** The board runs from USB-C or from a 3.7V LiPo battery on its 2-pin MX1.25 connector (check the polarity before plugging a battery in).  On battery, press the `PWR` button on the side to turn it on.  The firmware keeps the power switched on after you let go.  Hold `PWR` for 2 seconds to turn it off.
- **Images** are converted for the 412x412 screen automatically when you build, so the same image files work on every board (see [Adding or Changing Images](#adding-or-changing-images)).
- **Rotation:** The display controller can't rotate the picture by 90 degrees itself, so the ESP32 rotates it while drawing.  It's fast enough that you shouldn't notice.
- **Microphone:** used by the Voice feature (KITT and the voice ring).
- **SD card:** images on the card are shown along with the built-in ones.  See [Images on the SD Card](#images-on-the-sd-card-esp32-s3-board).
- **Speaker:** plays sound files from the SD card (see [Sounds on the SD Card](#sounds-on-the-sd-card-esp32-s3-board)).  The small round trimmer on the back of the board sets the speaker's volume, on top of the volume setting.
- **WiFi:** used by the Voice Lab and the Phone Remote, only while one of them is switched on.  They can't both run at once: switching one on stops the other.
- **Bluetooth:** used by the Lovense feature.  It starts the first time you tap Connect.
- **Not used yet:** the clock chip.
- **Housing:** there's a 3D printable housing for this board, and a travel case for it, in [`hardware/boards/waveshare_esp32s3_touch_lcd_1_46`](hardware/boards/waveshare_esp32s3_touch_lcd_1_46/).  See [`hardware/`](hardware/) for what fits which board.

### ESP32-S3-Touch-AMOLED-1.75 Notes

Support for this board is new and hasn't been tried on the board yet, so expect a few things to need adjusting.  It has everything the 1.46" board has, and the same features run on it.

- **Power:** The board runs from USB-C or from a 3.7V LiPo battery on its MX1.25 connector (check the polarity before plugging a battery in), and charges the battery at 300 mA.  Press `PWR` to turn it on, and hold it for 2 seconds to turn it off.
- **Images** are converted for the 466x466 screen automatically when you build.
- **Brightness:** an AMOLED has no backlight, so the Brightness setting dims the screen itself.  Black parts of the picture are truly off.
- **Rotation:** in the [speaker housing](hardware/boards/waveshare_esp32s3_touch_amoled_1_75/), the board is turned a quarter turn, so set Rotation to 90 degrees.
- **Microphones:** the board's two microphones are mixed together.  The gain is `MIC_GAIN_DB` in the board's `hal_board.c`.
- **Speaker:** the board has its own amplifier and speaker socket.  The microphones and the speaker share one audio clock, so sound is resampled to 32 kHz for the speaker, which is plenty for a small speaker.  `SPK_VOLUME` in `hal_board.c` sets how loud the board's amplifier goes, on top of the volume setting.
- **Level:** the accelerometer's axes are taken from the 1.46" board and haven't been checked on this one.  If the level reads the wrong way, see `rcat_hal_accel_read()` in `hal_board.c`.
- **Not used yet:** the clock chip, and the GPS on the -G version.
- **Housing:** there's a 3D printable speaker housing for this board in [`hardware/boards/waveshare_esp32s3_touch_amoled_1_75`](hardware/boards/waveshare_esp32s3_touch_amoled_1_75/).

## Software

I created this project because I was having trouble with all of the other existing projects intended for this hardware.  They all failed to initialize the touch screen over I2C or were otherwise unreliable on my hardware.  The Waveshare example projects seemed to work fine, though.  Therefore, I started from the [Waveshare example project that uses the LVGL GUI library](https://www.waveshare.com/wiki/RP2040-Touch-LCD-1.28#LVGL_Example_Demo), and it has since grown to support more boards.  The LVGL library opens up a lot of cool possibilities for effects if you want to spend the time working with it.  Take a look at the [LVGL documentation](https://docs.lvgl.io/) to get an idea of what it's capable of (this project uses LVGL v9.6).

If you want to change the images, you'll need to rebuild the firmware and reflash the device.

### Getting the code

LVGL and the sound file decoders are included as git submodules, so clone with `--recursive`:

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

Use an absolute path for `PICO_SDK_PATH`.  A relative path gets resolved from inside the build folder, not from where you run the command, which is confusing.

Each board gets its own build folder, so you can build for both without them getting in each other's way.

If you're building for the RP2350 version of the display:
```
cmake -S platforms/pico -B build-rp2350 -DBOARD=waveshare_rp2350_touch_lcd_1_28
cmake --build build-rp2350 -j
```

Otherwise, if you're building for the RP2040 display:
```
cmake -S platforms/pico -B build-rp2040 -DBOARD=waveshare_rp2040_touch_lcd_1_28
cmake --build build-rp2040 -j
```

When compilation is complete, you should have a file called `rcat-msa-lcd.uf2` in the build folder (`build-rp2350/rcat-msa-lcd.uf2` or `build-rp2040/rcat-msa-lcd.uf2`).  This is the image file you'll flash in the next step.

If CMake complains that `PICO_PLATFORM` doesn't match the board, the build folder was set up for the other board.  Use the other board's folder, or delete the folder and run the `cmake -S` command again.

#### Flashing

Connect the USB-C port of the RP2040 to your computer.  To flash a new image onto the hardware, press and hold the `BOOT` button.  Press and release `RESET`.  Now you can release `BOOT`.  The screen should be off.  A mass storage device should appear on the computer.  Copy the .uf2 file to this mass storage device.  When copying is complete, the RP2040 should reset and begin running the new image.

### Building for the ESP32-S3 boards

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

For the ESP32-S3-Touch-LCD-1.46:

```
idf.py -C platforms/esp-idf -B build-s3-146 -DBOARD=waveshare_esp32s3_touch_lcd_1_46 build
```

For the ESP32-S3-Touch-AMOLED-1.75:

```
idf.py -C platforms/esp-idf -B build-s3-175 -DBOARD=waveshare_esp32s3_touch_amoled_1_75 build
```

Give each board its own build folder, as here.  The first build downloads the board's display, touch and audio drivers from Espressif's component registry.  The examples below are for the 1.46"; for the 1.75" use `build-s3-175` instead.

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

## Choosing Features

The display's sections (images, voice, sound, level) can each be left out of the build.  Each platform has a file listing them:

- `platforms/pico/features.cmake` for the RP2040/RP2350 boards
- `platforms/esp-idf/features.cmake` for the ESP32 boards

Set a feature to `OFF` to leave it out, or `ON` to build it in, then build as usual.  The build picks up the change by itself and prints the features it's building, for example `-- Features: IMAGES, VOICE, SOUND`.  Settings is always built in.

A feature that needs hardware the board doesn't have (a microphone, a speaker, an accelerometer) hides itself even when it's built in.  The level is off for the RP2040/RP2350 boards, because their accelerometer isn't supported yet.

The ESP32 file also sets `RCAT_SPEAKER`, where the sound goes: `BUILT_IN` for the board's own speaker, or `EXTERNAL` for amplifiers wired to its expansion header, as in the [speaker housing](hardware/boards/waveshare_esp32s3_touch_lcd_1_46/README.md#speaker-housing).

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

The ESP32-S3 boards can also show images from their TF (micro SD) card, so you can change them without rebuilding the firmware.

1. Format the card as FAT32 (most cards come that way) and make a folder called `images` on it.  If there's no `images` folder, the top level of the card is used instead.
2. Copy PNG, JPG, BMP or animated GIF files into it.
3. Put the card in the board and switch it on.  It shows *Loading images...* while it reads the card.

The images from the card come first, sorted by file name, followed by the built-in images.  They can be picked in the *Startup* setting by their file name (without the extension).  If a card image has the same name as a built-in image, the card image is the one picked.  The card is only read when the board starts, so restart it after changing the card.

Images are sized for the screen the same way as the built-in images (see below).  Static images are resized once, when they're loaded.  Animated GIFs are scaled while they play, which costs a little time on every frame and looks slightly softer.  For the best results, prepare images on your computer first with `tools/prepare_sd.py`.  It resizes GIFs and static images ahead of time and marks them as prepared, so the board shows them as they are:

```
python3 tools/prepare_sd.py my_images/*.gif my_images/*.png --size 412 --out /media/you/SDCARD/images
```

Use the path where your card is mounted.  `--size` is the board's screen size: 412 for the ESP32-S3-Touch-LCD-1.46, or 466 for the ESP32-S3-Touch-AMOLED-1.75.

Limits:
- Up to 32 images are loaded from the card.
- Images can be up to 1024x1024 pixels.
- BMP files must be 16, 24 or 32-bit (not 1, 4 or 8-bit).  Files that can't be read are skipped, and the reason is printed in the monitor log.
- File names can be long, but only the first 23 characters (without the extension) are used for the *Startup* setting.
- Hidden files (like the `._` files macOS leaves on cards) are ignored.

### Sounds on the SD Card (ESP32-S3 board)

1. Make a folder called `sounds` on the card.
2. Copy WAV, MP3 or FLAC files into it.
3. Put the card in the board and switch it on.

The sounds are played in order of file name.  They're read from the card as they play, so they can be any length.  A sound that repeats (*Repeat one*) starts again straight after its end, so short sound effects loop without a gap.  MP3s loop without a gap if the encoder recorded the silence it adds to each end (LAME and ffmpeg do); otherwise there's a short pause at the loop.

Limits:
- Up to 100 sounds are found on the card.
- Any sample rate works.  Mono and stereo files both play, and files with more channels play their first two.
- Files that can't be played are skipped, and the reason is printed in the monitor log.
- The card is only read when the board starts, so restart it after changing the card.

### Patterns on the SD Card (ESP32-S3 board)

Put pattern files in a folder called `patterns` on the SD card, as `.txt` or `.pat` files.  Each file shows up on the Lovense feature's Patterns page, named after the file, after the six built-in patterns.

The files use Lovense's own pattern format, so patterns written for their developer tools work as they are:

```
V:1;F:v;S:100#0;4;8;12;16;20;16;12;8;4
```

- `F` is the motors the levels drive: `v` vibrate, `r` rotate, `p` the Max's air pump, and `t`, `f` or `o` thrusting.  Several can be listed, separated by commas.  A number after the letter picks one motor of that kind, so `v2` is the second vibrator on a device with two.  Leave `F` empty to drive every motor.
- `S` is how long each level lasts, in milliseconds.  It has to be at least 100, which is as fast as the devices take commands.
- After the `#` come the levels, 0 to 20, separated by `;`.  They can run over several lines.  When they run out, the pattern starts again.

A file can hold several of these one after another, each with its own `F` and `S`.  They play together, and each one repeats at its own length, so a short vibration pattern can run against a long, slow rotation.  For example:

```
V:1;F:v1;S:200#20;0;20;0;0;0
V:1;F:v2;S:100#0;2;4;6;8;10;12;14;16;18;20
```

The levels are scaled to the device's own range (0 to 3 for the air pump), and then by how high each ring is set on the Control page.  Up to 32 patterns are listed, built-in ones included.

### Phone Remote (ESP32-S3 board)

The *Phone Remote* page, the last one in Settings, lets people run the display from a phone's web browser, so nobody has to reach the display in the mask: pick what it shows, control the Lovense devices, and type messages onto the Terminal.  The phone needs no app.

1. Connect the devices on the display first (the Devices page).  The phone can connect them too, but the display is quicker.
2. On the *Phone Remote* page, switch it on.  The board starts a WiFi hotspot of its own, called `RCAT-` and four letters, and shows a QR code with its name and password under it.
3. On the phone, point the camera at the QR code and tap *Join*.  (Or join the network in the phone's WiFi settings and type the password.)
4. The remote's page opens on its own after a few seconds, the way a hotel's WiFi sign-in page does.  If it doesn't, open `http://192.168.4.1` in the phone's browser.

The page has three tabs:

- **Screen**: what's on the display now, with arrows to step through every page, and every feature's pages listed to jump straight to one.
- **Toys**: a slider for each motor of each connected device, the patterns and their speed, the reactions, a list of devices to connect, and a big **STOP** at the bottom that stops every device the moment it's touched.
- **Terminal**: type a message and *Send* it (or press Enter) to show it on the Terminal feature.  *Switch to it* (on to start with) brings the Terminal on screen when you send; *Add a line* puts the message under what's there instead of replacing it, dropping the oldest lines when it's full.  *Clear* empties it, and *Letter size* picks how big it's shown (see the Terminal feature above).  The last few messages are listed under it, to send again with a tap.  The display only has plain letters, numbers and punctuation, so curly quotes and dashes become straight ones, and anything else (emoji, accents) becomes `?`.

The display and the phones control the same things, so each shows what the others do.  Up to four phones can join at once, and the title on the display says how many have.  Adding `#screen`, `#toys` or `#term` to the page's address opens that tab.

The name and password are made up the first time the remote is switched on, and kept from then on.  Anyone who has the password can join, so if it's got around, switch the remote off, which drops every phone, and tap *New password*.  The new QR code lets the phones you want back in.  The hotspot is off at startup, and while it's on, the phone has no internet over WiFi (it may use mobile data for everything else).

On an iPhone, the page opens in a small sign-in window.  Leave it open, or tap *Done* and open `http://192.168.4.1` in Safari.  Android phones may show a "Sign in to network" notification while they're joined; tapping it opens the page.

The page is `core/app/remote_page.html`.  After changing it, run `python3 tools/gen_remote_page.py` to turn it into the C file that's built into the firmware.  The Terminal's font is LVGL's 8x8 unscii with each pixel drawn as a square block; `tools/gen_terminal_font.py` makes `core/ui/terminal_font.c` from it.

### Voice Effects (ESP32-S3 board)

The *Voice Effect* page (Voice feature) runs your voice through an effect on the board itself:

| Preset | Sound |
|---|---|
| Female | Your own intonation, raised in pitch, with the vowels raised less, for a natural female voice |
| Fembot | Female, with the pitch snapped to musical notes and a light chorus |
| Synthetic | Snapped pitch, a faint metallic ring and slight digital grit |
| Android | A monotone female robot |
| AI | Thinner, higher and snapped, with a quick shimmer |
| Robot | A monotone robot at a low pitch |
| Deep | Your voice, lower |
| Dalek | Ring modulated |
| Drone | Lower, ring modulated and slightly crushed |

The effect goes to the Voice Lab stream (`voice_monitor.py --listen effect` plays it) and, with *Play on speaker* switched on, to the board's speaker.  While it's on the speaker, the sound player waits.  *Play on speaker* is always off at startup: with the speaker facing into the mask, it feeds back.  An external speaker facing away from the mask works much better (the board's amplifier can drive one through the speaker pads; neither speaker wire is ground).

The presets are the same as the effects in `tools/voice_monitor.py`, so they can be tried and tuned on a computer first.  The pitch-following presets add about 15-20 ms of delay, the others less.

### Voice Lab (ESP32-S3 board)

The Voice Lab streams the board's microphone over WiFi to `tools/voice_monitor.py` on a computer, which plays it, records it and runs it through voice effects.  It's for trying out effects with the real microphone in the mask before building them into the firmware.

1. Put a file called `wifi.txt` on the SD card with your WiFi network's name and password (2.4 GHz networks only):
   ```
   ssid=My Network
   password=my password
   ```
2. Install the monitor's packages on the computer: `pip install numpy sounddevice` (on Linux, sounddevice also needs PortAudio: `sudo apt install libportaudio2`).
3. On the board, go to the last page of the Voice feature and switch the Voice Lab on.  It shows the board's address once it's connected.
4. On the computer, run `python3 tools/voice_monitor.py`.  It finds the board on the network, and the board shows *Streaming to* the computer's address.  If it isn't found, give the board's address: `python3 tools/voice_monitor.py --board 192.168.1.23`.

Some things to try:
- `--effect robot` (or `deep`, `dalek`, `drone`, `android`) plays the microphone through an effect.  Effects can be chained, like `--effect gate:-45,pitch:0.8,ring:40`.  Run with `--help` for the list.
- `--record take1.wav` saves the microphone as it is.  Then `--input take1.wav --effect robot` tries effects on the recording, without the board.
- `--listen effect` plays the board's own voice effect instead of the microphone.
- `--meter` prints the microphone level every second.  Play something on the speaker (Sound feature) to measure how much the microphone picks up.

WiFi adds a delay of around a tenth of a second, which is fine for judging an effect but odd if you listen to yourself live.  It's easier to have someone else listen, or to record and play back.  The Voice Lab is off whenever the board starts, and WiFi is off while it's off.

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
- `core/include/hal.h` - The small set of functions each board has to provide (display, touch, backlight, battery, settings storage, microphone, speaker, SD card, WiFi, Bluetooth).
- `core/features.cmake` - The list of optional features.  Each platform's `features.cmake` picks which ones to build.
- `platforms/pico/` - Build files for the Raspberry Pi RP2040/RP2350 boards.  `boards/*.cmake` lists the supported boards, and `boards/touch_lcd_1_28/` has the Waveshare drivers for the 1.28" round display.
- `platforms/esp-idf/` - Build files for the ESP32 boards.  `boards/` has one folder per board with its drivers, pin assignments and settings, and `components/rcat_esp32/` has the parts of the HAL that are the same on all of them (WiFi, the hotspot, Bluetooth LE, settings, the SD card and the IMU).
- `third_party/lvgl/` - The [LVGL](https://lvgl.io) graphics library (git submodule).
- `third_party/dr_libs/` - The [dr_libs](https://github.com/mackron/dr_libs) WAV, MP3 and FLAC decoders (git submodule).
- `tools/img2c.py` - Converts images and GIFs into C files.  The build runs it for every image in `imgs/`.
- `tools/prepare_sd.py` - Resizes and re-encodes images for an SD card.
- `tools/voice_monitor.py` - Plays and records the microphone stream from the Voice Lab, with voice effects.
- `tools/gen_lv_conf.py` - Generates the platforms' `lv_conf.h` files (LVGL settings) from the LVGL template.  Run it after updating the LVGL submodule.
- `imgs/` - The images.

Adding another display means adding a board folder with its HAL (`hal_board.c`) and its screen size, without copying the application code around.

## Adding a Feature

A feature is one of the sections you swipe up and down between, like Images or Level.  Each one lives in its own `core/ui/feature_<name>.c`, and [`core/ui/feature_level.c`](core/ui/feature_level.c) is a small, complete example to start from.

1. Create `core/ui/feature_<name>.c` with a `ui_feature_def_t` called `feature_<name>` (see [`core/ui/ui_internal.h`](core/ui/ui_internal.h)):
   - `build` adds the feature's pages with `ui_feature_add_page()`.  Return `false` to leave the feature out, for example when the board doesn't have the hardware it needs.
   - `on_navigate` is optional and is called whenever a different page comes on screen.  Use it to start and stop anything that only needs to run while the feature is showing.
   - Size things with `ui_px()`, which scales from the 240x240 layout to the board's screen.
   - `ui_add_startup_choice()` lets a page be picked in the Startup setting.
2. Add the name to `RCAT_FEATURES` in [`core/features.cmake`](core/features.cmake).
3. Add it to the list in [`core/ui/ui.c`](core/ui/ui.c), inside `#if RCAT_FEATURE_<NAME>`, above Settings.  Settings stays last so it can list every feature's startup choices.
4. Add a line for it to each platform's `features.cmake`, `ON` or `OFF`.

### Talking to the hardware

Code in `core/` never includes anything from pico-sdk or ESP-IDF.  It reaches the hardware through the functions in [`core/include/hal.h`](core/include/hal.h), which every board implements.

To add one:

1. Declare it in `hal.h`, with a comment saying what it does and what it returns when the board can't do it.
2. Implement it in every board's `hal_board.c`, under `platforms/esp-idf/boards/` and `platforms/pico/boards/`, or once for all the ESP32 boards in `platforms/esp-idf/components/rcat_esp32/` if it doesn't depend on the board.  A board without the hardware just returns `false` (or does nothing), and the feature using it hides itself.

The UI runs on a single LVGL loop.  Microphone and speaker callbacks run on the board's audio tasks, so they mustn't call LVGL.

### Settings and build options

- **Something people change on the display** goes in `settings_t` in [`core/app/settings.h`](core/app/settings.h).  Add new fields only at the end, so settings saved by older firmware still load.  Give the field a default in `set_defaults()`, check it in `load()`, and add a setter in [`core/app/settings.c`](core/app/settings.c).
- **Something fixed for a build**, like which speaker the ESP32 boards use, goes in the platform's `features.cmake`.  The build passes it to the code as a compile definition.

### Checking a change

Build every platform the change touches, using the commands in [Building for the RP2040/RP2350 boards](#building-for-the-rp2040rp2350-boards) and [Building for the ESP32-S3 boards](#building-for-the-esp32-s3-boards).  A change to `core/` or `hal.h` touches all of them.  Both builds should finish without warnings.  It's also worth building once with the new feature switched off.
