# MSA Millennium Housing for the ESP32-S3-Touch-AMOLED-1.75

A 3D printable housing that screws the [Waveshare ESP32-S3-Touch-AMOLED-1.75](https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm) into the voice port of an MSA Millennium mask, with a speaker in a pod below the display and a battery in a chamber behind it.  It's for the bare board, not the -B version that comes in its own case.

**Status: draft, not yet printed.**  It's modeled from Waveshare's 3D model and drawing of the board, and its clearance from the mask from a 3D scan.  The firmware doesn't support this board yet.

The board has its own amplifier, speaker socket, microphones and battery charger, so the speaker and the battery plug straight into it.  There are no other boards to wire in.

## How it goes together

- The board sits in a cup at the front, with its glass flush with the front, and screws to the cup's back plate with three M2 countersunk screws (about M2 x 5) into the standoffs on its back.
- It's turned a quarter turn from its usual way up, so its USB-C port faces right, out through the side of the housing, where a cable plugs straight in.  Its side buttons and micro SD slot are on the left, and its pin header is at the top.  Set the display's rotation to match in the settings.
- The speaker sits in a shallow pod below the display.  Right below the voice port is the mask's filter mount, so the pod is only as deep as the speaker needs, and the **chin** behind it slopes back to the body, over the mount.  From the scan, the chin clears the mount by about 4.7 mm at its closest, at the mount's top edge, and the pod by about 8 mm.
- The speaker goes in from the back of the pod before the chin goes on, and the adhesive frame on its front sticks it to the grille.  Its wire runs in a 2 mm gap behind it, into the chamber, to the board's speaker socket.
- The chamber behind the board holds a 2000 mAh battery, taped to the back.  The board's speaker and battery sockets stand up off the back of the board, and their plugs go in through openings in the back plate, from the chamber.
- The microphones are on the back of the board and listen backwards.  They hear through holes in the back plate, across the gap in front of the battery, and through an opening in the back into the voice port.
- The voice port thread is on a collar that turns freely on the back, so the housing can be held level while the collar is screwed in.  It's the same collar as the 1.46" speaker housing's, from [`../../masks/msa_millennium`](../../masks/msa_millennium/).

## Files

- `msa_speaker_housing.scad` - The design, in [OpenSCAD](https://openscad.org/).  All of the dimensions are named settings at the top.  Its preview shows the mask's filter mount and hose in red, to check the housing clears them.
- `board.scad` - The board's dimensions, measured from Waveshare's 3D model.
- `render.sh` - Renders all of the parts to STL files.

## Parts

Run `./render.sh` in this folder to render every part to the `stl/` folder, or `./render.sh body chin` for just those parts.  A single part can also be rendered by hand with, for example, `openscad -D 'part="body"' -o body.stl msa_speaker_housing.scad`.  The parts are all turned the right way up to print.

- **`body`** - The display cup and the speaker pod.  Like the 1.46" body, it'll probably print best standing on one of its sides with tree supports.
- **`chin`** - Closes the back of the pod and slopes back to the body.  It prints on the flat face that meets the pod, with no supports.
- **`back`** - Closes the chamber, with the posts the collar turns on.  Use 4 or more walls so the posts are solid.
- **`collar`**, **`washer`** and **`gasket`** - The turning collar, as on the 1.46" speaker housing.
- **`plungers`** - Two pins that press the board's side buttons through the wall.
- **`pod_test`** - The pod on its own, to check the speaker's fit and hear how it sounds.
- **`lens_fit`** - The front 3 mm of the body around the display.  The glass should drop in with a little play, but not rattle.  Adjust `clearance` if it doesn't.
- **`plunger_test`** - Plungers in three lengths, two of each, standing 1.3, 1.8 and 2.3 mm out of the housing.  Set `plunger_proud` to the one that works best.

### Check these first

These come from the 3D model, not from a print, so check them with test prints:

- The glass's fit in the cup (`clearance`), with `lens_fit`.
- That the micro SD card lines up with its slot.  If it doesn't, move the slot with `sd_shift`.  The 1.46" board's slot needed moving by 1 to 1.5 mm.
- That the speaker and battery plugs go into their sockets from behind, as the model suggests.  If the sockets turn out to face sideways, the openings in the back plate need to be bigger.

## Parts to buy

- A speaker module, 35 x 25 x 6.5 mm sealed box with an oval cone on the front, 4 ohm, with an MX1.25 2-pin plug (or one crimped on)
- A LiPo battery up to 50.5 x 34 x 9.9 mm with an MX1.25 2-pin plug.  Check its polarity against the board before plugging it in.
- M2 socket head screws: four M2 x 8 for the back, four M2 x 6 for the washer, and four M2 x 8 for the chin
- Three M2 x 5 countersunk screws for the board

## Putting it together

1. Push the plungers in from inside the display's pocket, then fit the board and screw it to the back plate.
2. Stick the speaker into its pocket from the back of the pod, then screw the chin on.  Leave the speaker's wire out through the opening at the inner end of the pocket, into the chamber.
3. Plug the speaker into the board's speaker socket, through the opening in the back plate.
4. Plug in the battery the same way, and tape it to the back.
5. Screw the back on with the M2 x 8 screws.
6. Put the gasket over the posts, then the collar, grip first.
7. Drop each half of the washer in through the threaded end, line its holes up with two posts, and screw it down with M2 x 6 screws.  Stop when the collar turns firmly but smoothly.

## Charging

The board's charger, an AXP2101, charges at 300 mA unless the firmware sets it higher, which is safe for any battery that fits.  The firmware can raise it to suit the battery, up to 1 A for a 2000 mAh battery rated at 0.5C.

## Credits and License

The housing is based on the 1.46" speaker housing in this repository, which in turn is based on [RP2040-LCD-1.28-MSA](https://www.printables.com/model/538771-rp2040-lcd-128-msa) by whyrlpool.

Like the designs it's based on, everything in this folder is licensed under the [Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License](https://creativecommons.org/licenses/by-nc-sa/4.0/).  This is separate from the license of the rest of the repository.

Board dimensions come from Waveshare's published drawing and 3D model of the ESP32-S3-Touch-AMOLED-1.75.
