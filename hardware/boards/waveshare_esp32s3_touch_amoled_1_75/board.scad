// The Waveshare ESP32-S3-Touch-AMOLED-1.75 (the bare board, not the -B
// version in its own case), shared by the housings in this folder. The MSA
// voice port thread is in hardware/masks/msa_millennium.
//
// Coordinates follow Waveshare's STEP model of the board: the front of the
// glass is at z = 0, the board extends towards -z, and x/y are as seen from
// the front with the USB-C port at the bottom (-y). The numbers below were
// measured from that model, and agree with Waveshare's dimension drawing.
//
// Seen from the front, the board has its side buttons at the top, either side
// of the micro SD slot, its 1x8 pin header down the right edge, and the
// speaker and battery sockets on the left. The two microphones are either
// side of the USB-C port, on the back of the board, listening backwards.
//
// The cup the board sits in has an inner radius of cup_ir. The openings below
// are cut into its wall, and the back plate the board screws to runs from
// plate_back to plate_front.

include <../../common/shapes.scad>
include <../../masks/msa_millennium/voice_port.scad>

/* [Board: Waveshare ESP32-S3-Touch-AMOLED-1.75] */
lens_d = 48.96;                     // cover glass
lens_t = 2.05;                      // the glass and the display behind it, which are the same size
board_r = 23.0;                     // the PCB, and the frame between it and the display
pcb_z = [-6.90, -5.70];
standoffs = [[14.70, 13.75], [14.70, -13.75], [-20.50, 0.00]];   // M2 threaded standoffs, 3.5 mm long
standoff_back_z = -10.43;           // the back of the standoffs, where the board rests on the back plate
header_box = [[17.48, -10.43], [19.88, 10.42]];     // 1x8 female header, reaches z = -12.70
header_back_z = -12.70;
// The speaker (H3) and battery (J1) sockets, MX1.25 2-pin. They stand up off
// the back of the board, so the plugs go in from behind.
spk_conn_box = [[-20.12, 2.67], [-15.92, 10.32]];
bat_conn_box = [[-20.12, -10.63], [-15.92, -2.98]];
conn_back_z = -11.70;
mic_xy = [[-10.96, -17.54], [11.74, -17.35]];      // MIC1 and MIC2, ports facing the back
usb_z = [-9.95, -6.95];             // USB-C port, centered on x = 0 at the bottom edge
usb_port_w = 9.58;
usb_port_front = 23.79;
sd_x = [-8.30, 7.80];               // micro SD socket, card goes in from the top edge (+y)
sd_z = [-6.30, -3.85];
sd_shift = [0, 0];                  // move the slot by this much (across, towards the front) if the
                                    // card doesn't line up. Not yet checked with a print
button_angles = [56, 124];          // side buttons, 34 degrees either side of the top
button_z = [-8.70, -6.50];
button_reach_r = 22.81;             // how far out the button actuators reach

/* [Housing] */
clearance = 0.2;                    // around the glass
wall = 2.0;
rim_above_glass = 0.3;              // the rim stands this far proud of the glass to protect it
plate_t = 2.0;                      // back plate the board screws to
screw_d = 2.2;                      // M2 clearance, snug so the screws center the board
screw_head_d = 4.0;                 // M2 countersunk head
header_grow = 0.5;                  // around the pin header
conn_grow = 0.5;                    // around the speaker and battery sockets
mic_hole = 2.0;
usb_access = true;                  // the board charges at 300 mA unless the firmware changes it,
                                    // which is safe for any battery that fits, so the port is open
usb_opening = [12.4, 6.6];          // width, height of the slot for the USB-C plug. The plug's
                                    // overmold has to fit into it to plug in all the way
sd_access = true;
sd_opening = [14.0, 3.4];           // width, height of the slot for the micro SD card

$fn = 128;

cup_ir = lens_d / 2 + clearance;
cup_or = cup_ir + wall;
cup_top = rim_above_glass;
plate_front = standoff_back_z;
plate_back = plate_front - plate_t;

// Holes through the back plate
module plate_holes()
{
    // Screws into the board's standoffs, countersunk from the back
    for (s = standoffs)
        translate([s[0], s[1], 0])
        {
            translate([0, 0, plate_back - 1]) cylinder(d = screw_d, h = plate_t + 2, $fn = 32);
            translate([0, 0, plate_back - 0.01])
                cylinder(d1 = screw_head_d, d2 = screw_d, h = (screw_head_d - screw_d) / 2, $fn = 32);
        }
    // The pin header and the sockets, which reach into the plate. The
    // sockets' plugs go in through these from behind.
    slab(header_box, plate_back - 1, plate_front + 1, grow = header_grow);
    for (b = [spk_conn_box, bat_conn_box]) slab(b, plate_back - 1, plate_front + 1, grow = conn_grow);
    // So the microphones can hear through the voice port
    for (m = mic_xy) translate([m[0], m[1], plate_back - 1]) cylinder(d = mic_hole, h = plate_t + 2, $fn = 24);
}

// Openings in the side wall. They reach well past the outside of the cup, so
// they also go through a thicker wall.
module wall_openings()
{
    // USB-C at the bottom: a rounded slot around the port
    if (usb_access) translate([0, -cup_ir + 3, (usb_z[0] + usb_z[1]) / 2])
        rotate([90, 0, 0])
            hull()
                for (s = [-1, 1])
                    translate([s * (usb_opening[0] - usb_opening[1]) / 2, 0, 0])
                        cylinder(d = usb_opening[1], h = 30, $fn = 48);
    // Micro SD slot at the top
    if (sd_access)
        translate([(sd_x[0] + sd_x[1]) / 2 + sd_shift[0] - sd_opening[0] / 2, cup_ir - 3,
                   (sd_z[0] + sd_z[1]) / 2 + sd_shift[1] - sd_opening[1] / 2])
            cube([sd_opening[0], 30, sd_opening[1]]);
}
