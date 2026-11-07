# MSA Millennium Housings for the ESP32-S3-Touch-LCD-1.46

3D printable housings that screw the [Waveshare ESP32-S3-Touch-LCD-1.46](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.46) into the voice port of an MSA Millennium mask.  They're for the version of the board **with the cover glass** (44.77 mm round lens).  There are two:

- **The simple housing** (`msa_housing.scad`): one printed part, holding the board and a small battery.
- **The speaker housing** (`msa_speaker_housing.scad`): the display between two speakers, with their amplifiers, a 2000 mAh battery and a USB-C charging port in a chamber behind, and a turning collar.

This README is about the design files.  For the whole build, with the parts to buy, wiring and putting it together, see the [build guide](../../../docs/hardware/waveshare_esp32s3_touch_lcd_1_46.md).

## Making the STL files

Run `render.sh` in this folder.  It needs [OpenSCAD](https://openscad.org/).

```
./render.sh simple      # the simple housing
./render.sh speaker     # the speaker housing
./render.sh             # both
```

The STL files go in the `stl/` folder, turned the right way up to print.  The parts render in parallel; the speaker housing's body and collar take the longest, around a minute.

To render just some parts, name them, like `./render.sh body back`.  A single part can also be rendered by hand, with for example `openscad -D 'part="body"' -o body.stl msa_speaker_housing.scad`.

## The simple housing

**Status:** the board, thread, micro SD slot and buttons have all been checked with full prints.

### How it goes together

- The board sits in a cup on the outside of the mask, with the glass flush with the front.  Its micro SD slot (left) and PWR/BOOT side buttons (right) stay reachable from the outside.  The buttons are pressed through flexible tabs in the wall, hinged at the front edge.
- The board screws to the cup's back plate, which carries the MSA voice port thread.  The space inside the thread (42 mm across, 8 mm deep) holds the battery, a 702030 LiPo (7 x 20 x 30 mm).  The PWR button turns the board on and off (hold it for 2 seconds to turn off), so there's no separate switch.
- The board's battery connector sits between the board and the back plate, so it can't be reached once the board is screwed in.  Plug a short extension cable into it first, and connect the battery to that.  The extension's wires come out over the speaker, through the opening in the back plate.
- There's a hole in the back plate behind the microphone.
- The USB-C port is covered by the wall, so the board can't be plugged in to charge by accident (see [Charge current](#charge-current)).  Take the board out to use USB, and unplug the battery first.  To print the housing with an opening for the port, set `usb_access = true`.

### Parts to print

- **`housing`** - The whole housing, in one piece.  It prints with its front on the bed.

### Screws

| Screws | How many | Where they go |
|---|---|---|
| M2 countersunk, about M2 x 5 | 3 | Through the back plate into the standoffs on the back of the board.  They go in from the back, through the inside of the thread, and their heads sit flush. |

## The speaker housing

**Status:** it's been fully assembled and worn, with the buttons and collar still being tuned.

The display sits in the middle, with a small speaker in a pod on each side, and a chamber behind for the battery, two MAX98357A amplifier boards and a USB-C charging port.  The voice port thread is on a collar that turns freely on the back, so the housing can be held level while the collar is screwed into the mask.  The board is turned a quarter turn, with its buttons at the top, so set the display's rotation to match in the settings.

The wiring to the board goes through the [J9 header adapter](header_adapter/), a small PCB soldered onto the board's pin header, so the board can come out without cutting anything.  `header_adapter = true` (the default) shapes the back plate for it.

### Parts to print

- **`body`** - The display cup and the speaker pods.  It prints well standing on one of its short sides with tree supports.  Printed on its back, the curved front edges show more layer lines.
- **`back`** - Closes the chamber, with the posts the collar turns on.  Use 4 or more walls so the posts are solid.
- **`collar`** - The voice port thread, with a knurled grip.
- **`washer`** - Holds the collar on.  It's in two halves so it fits in through the thread.
- **`gasket`** - A thin ring between the back and the collar, so the housing doesn't turn by itself.  Print it in TPU, or cut it from 1 mm rubber or craft foam.
- **`plungers`** - Two pins that press the board's side buttons through the top of the housing.
- **`spacers`** - Six short tubes the amplifier and charging port boards stand on.

### Screws

| Screws | How many | Where they go |
|---|---|---|
| M2 countersunk, about M2 x 5 | 3 | Through the display cup's back plate into the standoffs on the back of the board, from inside the chamber.  Their heads sit flush. |
| M2 x 10 socket head, with nuts and washers | 6 | Hold the two amplifiers and the USB-C breakout to the back: each goes through the back from outside, its head in a counterbore, then through a spacer and the board, with a nut on top. |
| M2 x 8 socket head | 4 | Hold the back onto the body.  They cut their own thread in the body. |
| M2 x 6 socket head | 4 | Hold the two halves of the washer to the ends of the collar's posts, inside the collar.  Stop when the collar turns firmly but smoothly. |

## Charge current

The board charges its battery at up to 2 A, set by R8 (82k) next to its charger chip.  That's more than either housing's battery should take, so it's worth changing R8 to a higher value.  The charge current is roughly 165 kΩ·A divided by R8:

| Housing | Battery | Suggested R8 | Charge current |
|---|---|---|---|
| Simple housing | 702030, about 400 mAh | 390k | about 0.4 A |
| Speaker housing | 2000 mAh | 180k | about 0.9 A |

In the speaker housing, the charging current also goes through the header adapter's SH socket, which shares it over two pins each way, at about 1 A a pin.  At 2 A that's right at their rating, so 0.9 A leaves plenty of margin.

R8 is a tiny 0201 part, so changing it is fine soldering work.  Measure the charge current with a USB meter afterwards.

## Test prints

You don't need to print these to build a housing: the design's already been checked and tuned with them.  They're for checking the fit on a different printer or with a different material, before printing a whole housing.  Render them with `./render.sh tests`.

- **`fit_test`** - The simple housing without the thread, to check the openings and buttons with the board fitted.
- **`lens_fit`** - The front 3 mm of the simple housing's cup.  The board's glass should drop in with a little play, but not rattle.  If it doesn't, adjust `clearance`.
- **`thread_test`** - Three thin thread rings at 99.4%, 99.7% and 100% of the original thread's diameter, numbered 1 to 3 by notches on the rim.  99.7% fit best, which is what `thread_scale` is set to (98% was too loose and 102% too tight).  `thread_scale` and the thread's beveled start, `thread_lead_in`, are in `../../masks/msa_millennium/voice_port.scad`, so they change the thread for every board's housing.
- **`pod_test`** - One of the speaker housing's pods on its own, to check the speaker's fit.
- **`plunger_test`** - The speaker housing's plungers in three lengths, two of each, standing 1.3, 1.8 and 2.3 mm out of the housing.  `plunger_proud` is set to the one that worked best.

## Files

- `msa_housing.scad` - The simple housing, in OpenSCAD.  All of the dimensions are named settings at the top.
- `msa_speaker_housing.scad` - The speaker housing.
- `board.scad` - The board's dimensions, shared by both housings.
- `render.sh` - Renders the parts to STL files.
- `header_adapter/` - The J9 header adapter PCB for the speaker housing: its KiCad files, Gerbers and 3D model.
- `travel_case/` - A case to carry the simple housing in.

The MSA voice port thread, the speaker housing's turning collar and the thread test rings are shared with the other boards' housings, in [`../../masks/msa_millennium`](../../masks/msa_millennium/).

## Travel case

There's a hinged case to carry the simple housing in, in [`travel_case`](travel_case/).

## Credits and License

This design is based on [RP2040-LCD-1.28-MSA](https://www.printables.com/model/538771-rp2040-lcd-128-msa) by whyrlpool, whose voice port thread was in turn adapted from the [Gas Mask VPU Cover (MSA compatible)](https://cults3d.com/en/3d-model/various/gas-mask-vpu-cover-msa-compatible).

Like the design it's based on, everything in this folder (apart from the travel case, which has its own license) is licensed under the [Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License](https://creativecommons.org/licenses/by-nc-sa/4.0/).  This is separate from the license of the rest of the repository.

Board dimensions come from Waveshare's published drawing and 3D model of the ESP32-S3-Touch-LCD-1.46.
