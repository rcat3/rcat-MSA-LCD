# J9 header adapter

A small PCB that solders onto the 2x10, 1.27 mm pin header (J9) on the back of the Waveshare ESP32-S3-Touch-LCD-1.46. It replaces the ribbon cable in the speaker housing (`hardware/enclosure/msa_1_46_speaker_housing.scad`). All the wiring to the back of the housing plugs into one 10-pin JST SH socket on it. To take the board out for a firmware update, you unplug that socket and the battery, and nothing has to be cut or desoldered.

**Status: not built yet.**  The layout passes KiCad's design rules check. It hasn't been fitted.

The first version (in the commit that added this folder) had the header's two rows swapped, a mirror image of the real header, so every pad sat on the other pin of its pair.  Don't fit a board from those Gerbers: it would connect the USB 5V to the battery.  This version has pin 1's row along the edge, next to the socket, and was checked pin by pin against the board's silkscreen.

## Files

- `header_adapter.kicad_pcb`, `header_adapter.kicad_pro` - the KiCad 6 board. There's no schematic, since the board is small enough to read straight off the layout.  The pinout is below.
- `fab/header_adapter_gerbers.zip` - Gerbers and drill files, ready to upload to a board house.

## Ordering

- 2 layers, 1.6 mm thick, 1 oz copper.  Any finish and any colour.
- The board is about 11 x 16 mm.
- The smallest hole is 0.3 mm and the smallest track is 0.2 mm, which every common board house can make.

## Parts

| Ref | Part | Notes |
|---|---|---|
| J2 | JST SM10B-SRSS-TB | 10-pin SH (1.0 mm) side-entry socket |
| R1 | 10k, 0603 | Pull-down on the amplifiers' SD line, so they stay off while the board boots |
| - | JST SHR-10V-S housing and SH crimp leads | The mating plug.  Pre-crimped leads are much easier than crimping your own. |

## Socket pinout

Pin 1 of the socket is the end nearest pin 1 of the header.

| SH pin | Signal | J9 pin | Wire it to |
|---|---|---|---|
| 1 | GND | 3, 4, 12 | USB-C breakout GND |
| 2 | BAT | 2 | Amplifier Vin, then on to the other amplifier |
| 3 | USB_5V | 1 | USB-C breakout 5V |
| 4 | USB_5V | 1 | USB-C breakout 5V |
| 5 | GND | 3, 4, 12 | USB-C breakout GND |
| 6 | GND | 3, 4, 12 | Amplifier GND, then on to the other amplifier |
| 7 | SD (GPIO1) | 11 | Amplifier SD, then on to the other amplifier |
| 8 | DIN (GPIO3) | 13 | Amplifier DIN, then on to the other amplifier |
| 9 | LRC (GPIO13) | 20 | Amplifier LRC, then on to the other amplifier |
| 10 | BCLK (GPIO12) | 18 | Amplifier BCLK, then on to the other amplifier |

Each SH contact is rated for about 1 A, so the charging current shares two pins each way.  At the board's 2 A charge rate that's right at the rating.  If you'd like more margin, change R8 on the Waveshare board to about 190k, which charges at about 1 A.

Wire each amplifier signal to the first amplifier's pad. Then run a short wire from that pad to the same pad on the second amplifier, with two wires in the one hole.  Keep the 220 uF capacitor across Vin and GND at each amplifier.

## Assembly

1. Solder J2 and R1 to the adapter first, while it's flat.
2. Hold the Waveshare board with its back towards you and the USB-C port to the left.  The header then runs left to right, with pin 1 (the dot, labelled 5V) at its lower left.  The lower row (5V, G, DN, DP and so on) runs along the board's edge, and the upper row (BAT, G, SCL, SDA and so on) is towards the middle.  Fit the adapter over the pins with its square pad, marked "1", on pin 1.  The socket is then below the header, facing the board's edge (the micro SD side).
3. Push it down until it sits on the header's plastic.  The pins stand about 1.8 mm out of the adapter.
4. Solder all 20 pins, including the unused ones, for strength.
5. Check with a meter that USB_5V, BAT and GND aren't shorted to each other, and that SD reads about 10k to GND.

Before soldering, make sure nothing else on the back of the board, inside the adapter's outline, is as tall as the header's plastic.

## In the housing

`header_adapter = true` in `msa_1_46_speaker_housing.scad` (the default) cuts the back plate to the adapter's outline and adds a notch for the plug, so a body printed before the adapter needs reprinting.  The plug goes in from the space below the battery.  The adapter's 3D model is in `hardware/enclosure/msa_1_46_header_adapter.scad`.
