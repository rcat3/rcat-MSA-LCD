# MSA Millennium Housing for the ESP32-S3-Touch-LCD-1.46

A 3D printable housing that screws the [Waveshare ESP32-S3-Touch-LCD-1.46](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.46) into the voice port of an MSA Millennium mask.  It's for the version of the board **with the cover glass** (44.77 mm round lens).

**Status: draft.**  The first full print fit well: the board sits snugly and the thread screws in cleanly.  This version tightens up the USB-C opening, moves the micro SD slot, and makes the button tabs easier to press.

## How it goes together

- The board sits in a cup on the outside of the mask, with the glass flush with the front.  Its USB-C port (bottom), micro SD slot (left) and PWR/BOOT side buttons (right) stay reachable from the outside.  The buttons are pressed through flexible tabs in the wall, hinged at the front edge.
- The board screws to the cup's back plate with three M2 countersunk screws (about M2x5) into the standoffs on the back of the board.  The screws go in from the back, through the inside of the threaded part.
- The back plate carries the MSA voice port thread.  The space inside the thread (42 mm across) holds the battery and power switch, like the 1.28" design.  The battery plug passes through an opening behind the board's battery connector.
- There are holes in the back plate behind the microphone and speaker.

## Files

- `msa_1_46_housing.scad` - The design, in [OpenSCAD](https://openscad.org/).  All of the dimensions are named settings at the top.
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

## Credits and License

This design is based on [RP2040-LCD-1.28-MSA](https://www.printables.com/model/538771-rp2040-lcd-128-msa) by whyrlpool, whose voice port thread was in turn adapted from the [Gas Mask VPU Cover (MSA compatible)](https://cults3d.com/en/3d-model/various/gas-mask-vpu-cover-msa-compatible).

Like the design it's based on, everything in this folder is licensed under the [Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License](https://creativecommons.org/licenses/by-nc-sa/4.0/).  This is separate from the license of the rest of the repository.

Board dimensions come from Waveshare's published drawing and 3D model of the ESP32-S3-Touch-LCD-1.46.
