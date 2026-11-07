# Lovense

*Boards with Bluetooth: the ESP32-S3 boards*

The Lovense feature controls up to three Lovense devices at once, over Bluetooth.  The connections stay up while you look at other features, so the levels stay as they were set.  Everything here can also be done from a phone, with the [Phone Remote](phone-remote.md).

Its pages, left to right:

- **Control**: a ring around the edge for each of the device's motors (vibration, rotation, the Max's air pump and so on), with the level on its knob, 0 to 20 for most.  Drag a knob around its ring to set the level.  Only the knobs respond, so you can swipe from anywhere else.  The device's name is in the middle, with arrows either side to step through the connected devices.  **STOP** stops every device, and any pattern, as soon as you touch it.
- **Devices**: turn your devices on and they show up here while it's on screen.  Tap one to connect (it turns green), or tap it again to stop it and disconnect.  Bluetooth only starts the first time it's needed.
- **Patterns**: tap a pattern to play it, and tap it again to stop it.  The arrows at the top pick which device it plays on, or all of them, and **-**/**+** at the bottom set the speed.  While a pattern plays, each ring on the Control page sets how high that motor goes, and a thin line inside the ring shows the level as it plays.  Six patterns are built in, and you can add your own (see [Patterns on the SD card](#patterns-on-the-sd-card)).
- **React to**: drives the devices from your voice or how you move, on top of any pattern.  *Voice* follows how loud you talk.  *Moving* runs while you move and stops soon after you stop.  *Still* runs only once you've held still with the display upright for a second (set the display's [Rotation](settings.md#rotation) first, so upright is right).  *Motion* gets stronger the more you move, and *Tilt* the further you tilt from upright.  Tap the one that's on to turn it off.  The line under the list shows what it's sensing.  As with patterns, the rings on the Control page set how high each motor goes.  **STOP** turns the reaction off too.
- **Options**: *Motor Hold*: on, a motor keeps its level when you let go of its knob; off, it springs back to 0.

## Patterns on the SD card

Put pattern files in a folder called `patterns` on the SD card, as `.txt` or `.pat` files.  Each file shows up on the Patterns page, named after the file, after the six built-in patterns.

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
