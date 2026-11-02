# MSA Millennium Housing for the ESP32-S3-Touch-LCD-1.46

A 3D printable housing that screws the [Waveshare ESP32-S3-Touch-LCD-1.46](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.46) into the voice port of an MSA Millennium mask.  It's for the version of the board **with the cover glass** (44.77 mm round lens).

**Status: draft.**  The board, thread, micro SD slot and buttons have all been checked with full prints.

## How it goes together

- The board sits in a cup on the outside of the mask, with the glass flush with the front.  Its micro SD slot (left) and PWR/BOOT side buttons (right) stay reachable from the outside.  The buttons are pressed through flexible tabs in the wall, hinged at the front edge.
- The USB-C port is covered by the wall.  The board charges its battery at up to 2 A, far more than a small LiPo is rated for, so this stops it being plugged in by accident.  Take the board out to use USB, and unplug the battery first.  To print the housing with an opening for the port, set `usb_access = true`.
- The board screws to the cup's back plate with three M2 countersunk screws (about M2x5) into the standoffs on the back of the board.  The screws go in from the back, through the inside of the threaded part.
- The back plate carries the MSA voice port thread.  The space inside the thread (42 mm across, 8 mm deep) holds the battery, a 702030 LiPo (7 x 20 x 30 mm).  The PWR button turns the board on and off (hold it for 2 seconds to turn off), so there's no separate switch.
- The board's battery connector sits between the board and the back plate, so it can't be reached once the board is screwed in.  Plug a short extension cable into it first, and connect the battery to that.  The extension's wires come out over the speaker, through the opening in the back plate.
- There's a hole in the back plate behind the microphone.

## Files

- `msa_1_46_housing.scad` - The design, in [OpenSCAD](https://openscad.org/).  All of the dimensions are named settings at the top.
- `msa_1_46_speaker_housing.scad` - The speaker housing (see below).
- `render.sh` - Renders all of the speaker housing's parts to STL files.
- `msa_1_46_board.scad` - The board's dimensions and the thread, shared by both housings.
- `vendor/vpu_thread.stl` - The MSA thread, taken from the 1.28" design (see Credits).  It's been cleaned up into a single solid, and the original's inner lip has been removed so the inside is a plain 42 mm bore.

To make the STL files to print:

```
openscad -o housing.stl msa_1_46_housing.scad
openscad -D 'part="fit_test"' -o fit_test.stl msa_1_46_housing.scad
openscad -D 'part="lens_fit"' -o lens_fit.stl msa_1_46_housing.scad
openscad -D 'part="thread_test"' -o thread_test.stl msa_1_46_housing.scad
```

Everything is already turned the right way up to print, with the front of the housing on the print bed.

## Test prints

Print these first.  They're small and quick, and they save printing the whole housing several times.

- **`fit_test`** - The whole housing without the thread.  It's much quicker to print, and is enough to check the openings and buttons with the board fitted.
- **`lens_fit`** - The front 3 mm of the cup.  The board's glass should drop in with a little play, but not rattle.  Adjust `clearance` if it doesn't.
- **`thread_test`** - Three thin thread rings at 99.4%, 99.7% and 100% of the original thread's diameter, each with a bar across the middle to grip.  The notches on the rim count 1 to 3 in that order.  Try each one in the mask's voice port and set `thread_scale` to the best one.  99.7% fit best, which is what `thread_scale` is set to.  (An earlier round found 98% too loose and 102% too tight.)

The start of the thread is beveled (`thread_lead_in`) so it catches more easily when you screw it in.

## Speaker housing

`msa_1_46_speaker_housing.scad` is an all-in-one version: the display in the middle, a small speaker in a pod on each side, and a chamber behind for a bigger battery, two MAX98357A amplifier boards and a USB-C charging port.  The voice port thread is on a separate collar that turns on the back, so the housing can be held level while the collar is screwed into the mask.

**Status: draft.**  It's been fully assembled and worn, with the buttons and collar still being tuned.

The board is turned a quarter turn, with its buttons at the top, so set the display's rotation to match in the settings.

### Parts

Run `./render.sh` in this folder to render every part to the `stl/` folder, or `./render.sh body back` for just those parts.  It needs [OpenSCAD](https://openscad.org/).  A single part can also be rendered by hand with, for example, `openscad -D 'part="body"' -o body.stl msa_1_46_speaker_housing.scad`.  The parts are all turned the right way up to print.

- **`body`** - The display cup and the speaker pods.  It prints well standing on one of its short sides with tree supports.  Printed on its back, the curved front edges show more layer lines.
- **`back`** - Closes the chamber, with the posts the collar turns on.  Use 4 or more walls so the posts are solid.
- **`collar`** - The voice port thread, with a knurled grip.
- **`washer`** - Holds the collar on.  It's in two halves so it fits in through the thread.
- **`gasket`** - A thin ring between the back and the collar, so the housing doesn't turn by itself.  Print it in TPU, or cut it from 1 mm rubber or craft foam.
- **`plungers`** - Two pins that press the board's side buttons through the top of the housing.
- **`spacers`** - Six short tubes the amplifier and charging port boards stand on.
- **`pod_test`** - One speaker pod on its own, to check the speaker's fit.
- **`plunger_test`** - Plungers in three lengths, two of each, standing 1.3, 1.8 and 2.3 mm out of the housing.  Set `plunger_proud` to the one that works best.

### Parts to buy

- Two speaker modules, 35 x 25 x 6.5 mm sealed boxes with an oval cone on the front
- A LiPo battery up to 50.5 x 34 x 9.9 mm, rated to charge at 1C (the board charges at 2 A), with a JST 1.25 plug
- Two MAX98357A amplifier boards
- A USB-C breakout with 5.1k resistors from CC1 and CC2 to GND
- A 2x10 ribbon cable with 1.27 mm pitch sockets, cut to about 25-30 mm from the plug
- M2 socket head screws: four M2 x 8 for the back, four M2 x 6 for the washer, and six M2 x 10 with nuts and washers for the boards

### Putting it together

1. Push the plungers in from inside the display's pocket, then fit the board.
2. Stick each speaker into its pocket from the back.  The adhesive frame on its front holds it to the grille.
3. Screw the amplifiers and the USB-C breakout to the back: each M2 x 10 goes through the back from outside, then a spacer, the board and a nut.  Tape the battery to the back, label side down with its leads at the lower left (seen from the front).
4. Wire it up, then screw the back on with the M2 x 8 screws.
5. Put the gasket over the posts, then the collar, grip first.
6. Drop each half of the washer in through the threaded end, line its holes up with two posts, and screw it down with M2 x 6 screws.  Stop when the collar turns firmly but smoothly.

### Wiring

Everything but the battery connects through the ribbon on the board's 2x10 header.  The battery plugs into the board's own battery socket.

| Header pin | Signal | Goes to |
|---|---|---|
| 1 | USB_5V | USB-C breakout VBUS |
| 2 | BAT | Vin on both amplifiers |
| 3 | GND | USB-C breakout GND |
| 4, 12 | GND | GND on both amplifiers |
| 11 | GPIO1 | SD on both amplifiers, and a 10k resistor to GND |
| 13 | GPIO3 | DIN on both amplifiers |
| 18 | GPIO12 | BCLK on both amplifiers |
| 20 | GPIO13 | LRC on both amplifiers |

- Each speaker connects to its amplifier's + and - outputs, neither of which is ground.  Leave GAIN unconnected.
- BAT is connected straight to the battery, so the 10k resistor keeps the amplifiers switched off while the board is off.
- Put a 100-220 uF capacitor across Vin and GND at each amplifier.
- Don't plug into the board's own USB-C port and the housing's at the same time.
- The firmware sends the sound to the amplifiers when `RCAT_SPEAKER` is `EXTERNAL` in `platforms/esp-idf/features.cmake`, which is the default.  It switches them on with SD only while sound is playing.

## Travel case

There's a hinged case to carry the housing in, in [`../travel-case`](../travel-case/).

## Credits and License

This design is based on [RP2040-LCD-1.28-MSA](https://www.printables.com/model/538771-rp2040-lcd-128-msa) by whyrlpool, whose voice port thread was in turn adapted from the [Gas Mask VPU Cover (MSA compatible)](https://cults3d.com/en/3d-model/various/gas-mask-vpu-cover-msa-compatible).

Like the design it's based on, everything in this folder is licensed under the [Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License](https://creativecommons.org/licenses/by-nc-sa/4.0/).  This is separate from the license of the rest of the repository.

Board dimensions come from Waveshare's published drawing and 3D model of the ESP32-S3-Touch-LCD-1.46.
