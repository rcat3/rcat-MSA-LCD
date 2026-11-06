# MSA Millennium

What every housing for the MSA Millennium mask shares, whichever board is in it.

## Files

- `voice_port.scad` - The voice port thread (`vpu_thread()`), its fit settings, and `thread_test()`, the thin rings for finding the right fit.
- `vpu_thread_mesh.scad` - Loads the thread's mesh.  It's in a file of its own so the mesh is found from any board's folder.
- `vendor/vpu_thread.stl` - The thread, taken from the 1.28" design (see Credits).  It's been cleaned up into a single solid, and the original's inner lip has been removed so the inside is a plain 42 mm bore.
- `turning_collar.scad` - The collar that carries the thread and turns freely on the back of a housing, with its washer, gasket and the posts it turns on.  The housing can then be held level while only the collar is screwed in.
- `keepout.scad` - The parts of the mask around the voice port that a housing mustn't hang into, for previews.

Include these from a housing with, for example, `include <../../masks/msa_millennium/voice_port.scad>`.

## The thread

`thread_scale` is 99.7%, which fit best in a mask out of three test rings at 99.4%, 99.7% and 100% (an earlier round found 98% too loose and 102% too tight).  The start of the thread is beveled (`thread_lead_in`) so it catches more easily.  Changing either changes every housing's thread.

The thread reaches 8 mm into the port.  When it's screwed all the way in, the back of the collar's grip sits on the front of the port's threaded rim, so a housing's `z_grip` is where the rim ends up.

## Around the voice port

These measurements come from a 3D scan of a mask on a head, with the eyes level.  They're in the voice port's coordinates: the origin is the center of the front of its threaded rim, z points out of the mask, and y points up.  The mask sat a few degrees crooked in the scan, so allow a few degrees either way.

- **The threaded rim** is flat and round, 28.4 mm in radius at its front edge, and stands about 10 mm out of the mask around it.
- **The filter mount** is right below the port.  Its top edge is about 40 mm below the port's center, where it's only about 4 mm out from the rim.  Its face slopes outward from there, to 13.5 mm out from the rim about 57 mm below the center.  It's about 50 mm wide.
- **The hose** loops from the mount out to the right, seen from the front, up to 12.5 mm out from the rim, between 50 and 80 mm below the center.

Anything within about 38 mm of the port's center clears all of it.  Further down, a housing has to stay in front of the mount.  The speaker pod under the ESP32-S3-Touch-AMOLED-1.75's housing does this with a sloped underside.

`keepout.scad` has these as simple shapes, `mask_keepout()`, which are a little bigger than the scan, never smaller.  A housing's preview shows them with `translate([0, 0, z_grip]) mask_keepout();`.

## Credits and License

The thread is from [RP2040-LCD-1.28-MSA](https://www.printables.com/model/538771-rp2040-lcd-128-msa) by whyrlpool, which adapted it from the [Gas Mask VPU Cover (MSA compatible)](https://cults3d.com/en/3d-model/various/gas-mask-vpu-cover-msa-compatible).

Like the design it's from, everything in this folder is licensed under the [Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License](https://creativecommons.org/licenses/by-nc-sa/4.0/).  This is separate from the license of the rest of the repository.
