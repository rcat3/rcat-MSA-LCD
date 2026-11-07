// The Waveshare RP2040-Touch-LCD-1.28 and RP2350-Touch-LCD-1.28, which are
// the same shape. The MSA voice port thread is in hardware/masks/msa_millennium.
//
// Coordinates are as seen from the front, centered on the round glass: the
// front of the glass is at z = 0, the board extends towards -z, and the USB-C
// port is at the bottom (-y). The numbers were measured from Waveshare's 3D
// model of the RP2040 board. Their model of the RP2350 board leaves out the
// touch panel, the battery socket and the pin header, so it's taken to match
// the RP2040 board apart from its thicker PCB.
//
// The board has no mounting holes. Seen from the front, the glass is round,
// and behind it the board is a teardrop, with the USB-C port in its point.
// The battery socket is on the left edge of the back, and its plug goes in
// from the edge. The BOOT and RESET buttons and the pin header are near the
// top of the back.

include <../../common/shapes.scad>
include <../../masks/msa_millennium/voice_port.scad>

board = "rp2040";           // "rp2040" or "rp2350"

/* [Board] */
glass_r = 17.84;            // the round cover glass on the front
glass_t = 0.5;              // the glass's edge, in front of the wider parts behind it
// Everything behind the glass, seen from the front: a circle with the
// teardrop's point below it
outline_r = 19.25;
outline_point = [[-13.97, -13.25], [-6.46, -21.02], [-4.48, -22.10],
                 [4.48, -22.10], [6.46, -21.02], [13.97, -13.25]];
pcb_r = 18.77;
// From the front of the glass to the back of the PCB. The RP2350 board's PCB
// is 1.6 mm thick instead of 1.0, so it's thicker by 0.6 mm (not yet checked
// on a real board).
pcb_back_z = board == "rp2350" ? -5.5 : -4.9;
back_height = 3.4;          // the tallest part on the back (the battery socket), from the PCB
// Parts on the back: [[x0, y0], [x1, y1], height from the PCB]
battery_socket = [[-17.80, -4.60], [-12.60, 3.05], 3.4];    // the plug goes in from -x
usb = [[-4.80, -22.10], [4.79, -14.57], 3.25];              // its front is at y = -22.10
usb_z = [-0.91, 3.25];      // the USB-C port, from the back of the PCB
header = [[-7.17, 7.40], [7.18, 12.50], 3.13];
buttons = [[[8.63, 8.58], [13.28, 11.88], 2.5], [[-13.28, 8.58], [-8.63, 11.88], 2.5]];
crystal = [[5.86, -13.54], [11.24, -8.16], 2.0];
active_r = 16.2;            // the picture, 32.4 mm across
// Clear spots on the back near the edge, where the housing can hold the
// board up. They're clear of everything on both boards.
pcb_rest_angles = [90, 210, 330];

module board_outline_2d(grow = 0)
{
    offset(r = grow)
        hull()
        {
            circle(r = outline_r, $fn = 128);
            polygon(outline_point);
        }
}

// A rough model of the board, for previews
module board_model()
{
    color("black") translate([0, 0, -glass_t]) cylinder(r = glass_r, h = glass_t, $fn = 128);
    color("dimgray") translate([0, 0, pcb_back_z]) linear_extrude(-pcb_back_z - glass_t) board_outline_2d();
    color("silver")
        for (b = concat([battery_socket, usb, header, crystal], buttons))
            translate([b[0][0], b[0][1], pcb_back_z - b[2]]) cube([b[1][0] - b[0][0], b[1][1] - b[0][1], b[2]]);
}
