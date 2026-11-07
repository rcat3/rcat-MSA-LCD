# MSA Millennium Housing for the RP2040/RP2350-Touch-LCD-1.28

A 3D printable housing that screws the [Waveshare RP2040-Touch-LCD-1.28](https://www.waveshare.com/wiki/RP2040-Touch-LCD-1.28) or [RP2350-Touch-LCD-1.28](https://www.waveshare.com/wiki/RP2350-Touch-LCD-1.28) into the voice port of an MSA Millennium mask, with the battery and a power switch inside.

This README is about the design files.  For the whole build, with the parts to buy, wiring, putting it together and charging, see the [build guide](../../../docs/hardware/waveshare_touch_lcd_1_28.md).

**Status: draft, not yet printed.**  It's modeled from Waveshare's 3D model of the RP2040 board.  Their model of the RP2350 board leaves out the touch panel and the battery socket, so it's taken to be the same apart from its thicker PCB.

## How it goes together

It's two printed parts and no screws:

- **The body** is one piece, with the voice port thread on its back.  Nothing sticks out of the back, so it screws all the way in and sits flat on the port.
- **The board** drops into the body from the front and rests on three small pads behind the edge of its PCB.
- **The battery** sits behind the board, in a closed compartment inside the thread, in a shallow cradle in the floor so it can't slide around.  There's 1 mm between the top of the battery and the tallest parts on the back of the board, for a piece of 1 mm foam tape on top of the battery.  The board presses it down, so the battery stays put and nothing on the board can rub against it.
- **A slide switch** in the side wall turns the battery on and off.  It sits in a pocket in the wall, with its lever out through a slot, so there's nothing to drill.  A catch holds it in: the roof of the pocket is a little tongue with a hook under its end, which the switch pushes up as it goes in, and which drops down behind it.
- **The bezel** snaps over the front of the body and holds the board in by the edge of its glass.  To take it off, pry it up with a fingernail at the notch at the bottom.
- **The USB-C port** is reachable through a pocket in the right side (seen from the front), for charging and flashing.  The board is turned a quarter turn so the port isn't facing the mask's filter mount, below the voice port.  Set the display's Rotation to 90 degrees to match.

The housing stands about 12 mm out of the mask: 11.6 mm with the RP2040 board, 12.2 mm with the RP2350 board.

### Making it sit level

The whole housing turns as it's screwed in, so how it ends up sitting depends on where the thread starts, which is the same on every Millennium mask.  `thread_phase` turns the thread on the body so the housing ends up level.

1. Print `phase_test`, which is just the thread with a scale around its front.
2. Screw it all the way into the mask (with the gasket, if you'll use one).
3. Hold the mask upright and read the number at the top of the scale.
4. Set `thread_phase` to that number in `msa_housing.scad`, and print the body.

## Files

- `msa_housing.scad` - The design, in [OpenSCAD](https://openscad.org/).  All of the dimensions are named settings at the top.  Its preview shows the board, the battery, and the mask's filter mount in red.
- `board.scad` - The boards' dimensions.  Set `board` to `"rp2040"` or `"rp2350"` here.
- `render.sh` - Renders all of the parts to STL files in `stl/`.
- `travel_case/` - A hinged case to carry the housing in, with the board in it.

## Parts

Set `board` in `board.scad` to `"rp2040"` or `"rp2350"` first, then run `./render.sh` in this folder to render the housing's parts to the `stl/` folder.  It needs [OpenSCAD](https://openscad.org/).  `./render.sh tests` renders `phase_test` and `fit_test`, and `./render.sh body bezel` just those parts.  A single part can also be rendered by hand with, for example, `openscad -D 'part="body"' -o body.stl msa_housing.scad`.  The parts are all turned the right way up to print.

There are no screws: the bezel snaps on and the switch clicks into place.

- **`body`** - Printed back down.  The pocket for the USB-C plug needs a little support.
- **`bezel`** - Printed front down.  PETG is a good choice, as its skirt flexes to snap on.
- **`gasket`** - Optional.  A thin ring between the body and the voice port, to seal it.  Print it in TPU, or cut it from 1 mm rubber or craft foam.
- **`phase_test`** - The thread with a scale on its front, for `thread_phase` (see above).
- **`fit_test`** - The front of the body without the thread, to check the board, the switch and the bezel fit before printing the whole body.

### Check these first

These come from the 3D model, not from a print, so check them with `fit_test`:

- The board's fit in its pocket (`clearance`) and on its pads.  If the board rattles once the bezel is on, the pads are too low.  If the bezel won't snap on, they're too high.
- With the RP2350 board, its thickness: from the front of the glass to the back of the PCB should be about 5.5 mm (`pcb_back_z` in `board.scad`).
- The switch's fit in its pocket (`switch_body`), that the lever moves freely in its slot, and that the catch clicks behind it (`catch_hook`).  If the catch is too stiff or breaks, print the body in PETG, which bends more than PLA before it breaks.
- How firmly the bezel snaps on (`snap_bead`).

## Parts to buy, wiring and putting it together

See the [build guide](../../../docs/hardware/waveshare_touch_lcd_1_28.md).

## Charge current

The boards charge their battery at 1 A, set by R15 (160k) next to the charger chip, which is several times what a 250-310 mAh battery should take.  Changing R15 to about 560k brings it down to about 0.3 A.  The [build guide](../../../docs/hardware/waveshare_touch_lcd_1_28.md#charging) has the details and other values.

## Travel case

There's a hinged case to carry the housing in, in [`travel_case`](travel_case/).

## Credits and License

The voice port thread is from [RP2040-LCD-1.28-MSA](https://www.printables.com/model/538771-rp2040-lcd-128-msa) by whyrlpool, which adapted it from the [Gas Mask VPU Cover (MSA compatible)](https://cults3d.com/en/3d-model/various/gas-mask-vpu-cover-msa-compatible).

Like the design it's from, everything in this folder (apart from the travel case, which has its own license) is licensed under the [Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License](https://creativecommons.org/licenses/by-nc-sa/4.0/).  This is separate from the license of the rest of the repository.

Board dimensions come from Waveshare's 3D models of the RP2040-Touch-LCD-1.28 and RP2350-Touch-LCD-1.28.
