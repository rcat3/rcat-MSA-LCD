// A small PCB soldered onto the 1.46" board's 2x10 pin header, in place of a
// ribbon cable. It carries a 10-pin JST SH (1.0 mm) side-entry socket, so the
// wiring to the back unplugs from the board, and a 10k pull-down on the
// amplifiers' SD line.
//
// The header's pins go through plated holes in the PCB and are soldered on
// its back, so there's no socket contact between the board and the PCB. The
// PCB rests on the header's plastic, and the pins stand 1.8 mm out of its
// back. The SH socket sits on the PCB's back, beside the pins, with its
// opening at the edge of the PCB, facing the board's micro SD side. In the speaker housing that's
// straight down, so the plug goes in from the open space below the battery.
//
// The PCB stays on the board, and has to come out through the display's cup
// with it, so it fits inside adapter_r.
//
// The PCB itself is the KiCad project in hardware/header_adapter, and the
// numbers here match it.
//
// Everything here is in the board's coordinates (see msa_1_46_board.scad).

header_pin_tip = -12.95;            // where the header's pins end
header_pin_length = 3.4;            // how far they stand out of the header's plastic (measured)
adapter_t = 1.6;                    // PCB thickness
adapter_z = header_pin_tip + header_pin_length;     // its front face, resting on the header's plastic
adapter_r = 22.3;                   // it has to pass through the cup (radius cup_ir) with the board
adapter_edge = -21.6;               // the edge the socket's opening is on (towards the micro SD side),
                                    // where the circle doesn't trim it
adapter_inner = -10.4;              // the opposite edge
adapter_sock_front = -20.85;        // the socket's opening
adapter_w = 16.0;                   // across, along the header
adapter_corner = 1.0;
adapter_pitch = 1.27;

// JST SM10B-SRSS-TB: width along the row, depth from the opening back to the
// end of its solder pads, height off the PCB
adapter_conn = [12.8, 5.1, 2.95];
// The plug, and how far it reaches out past the PCB's edge with its wires bent back
adapter_plug = [12.0, 4.0, 2.7];
adapter_plug_reach = 6.5;

adapter_hc = box_center(header_box);
adapter_back = adapter_z - adapter_t;

function adapter_pins() = [for (r = [0, 1], i = [0 : 9])
    [adapter_hc[0] + (r - 0.5) * adapter_pitch, adapter_hc[1] + (i - 4.5) * adapter_pitch]];

module adapter_2d(grow = 0)
{
    intersection()
    {
        circle(r = adapter_r + grow, $fn = 128);
        translate([adapter_edge - grow, adapter_hc[1] - adapter_w / 2 - grow])
            offset(r = adapter_corner) offset(delta = -adapter_corner)
                square([adapter_inner - adapter_edge + 2 * grow, adapter_w + 2 * grow]);
    }
}

// The PCB with the header's pins through it, the socket, and the plug in it.
// only picks one piece ("pcb", "pins", "socket", "plug" or "wires"), for drawings.
module header_adapter(plug = true, only = "")
{
    function show(p) = only == "" || only == p;
    if (show("pcb")) color("darkgreen") difference()
    {
        translate([0, 0, adapter_back]) linear_extrude(adapter_t) adapter_2d();
        for (p = adapter_pins()) translate([p[0], p[1], adapter_back - 1]) cylinder(d = 0.7, h = adapter_t + 2, $fn = 16);
    }
    // Pins, and solder fillets on the back
    if (show("pins")) for (p = adapter_pins())
    {
        color("gold") translate([p[0] - 0.2, p[1] - 0.2, header_pin_tip]) cube([0.4, 0.4, adapter_z + 3 - header_pin_tip]);
        color("silver") translate([p[0], p[1], adapter_back - 0.6]) cylinder(d1 = 0.5, d2 = 1.0, h = 0.6, $fn = 16);
    }
    // The pull-down resistor (0603), beside the pins on the inner side
    if (show("pcb")) color("black") translate([-11.9 - 0.4, -0.8, adapter_back - 0.45]) cube([0.8, 1.6, 0.45]);
    // The socket, opening flush with the edge
    if (show("socket")) color("white") translate([adapter_sock_front, adapter_hc[1] - adapter_conn[0] / 2, adapter_back - adapter_conn[2]])
        difference()
        {
            cube([adapter_conn[1], adapter_conn[0], adapter_conn[2]]);
            translate([-0.01, 1.0, 0.4]) cube([3.0, adapter_conn[0] - 2.0, adapter_conn[2] - 0.8]);
        }
    if (plug)
    {
        x0 = adapter_sock_front + 2.5;      // how far the plug goes into the socket
        if (show("plug")) color("ivory") translate([x0 - adapter_plug[1], adapter_hc[1] - adapter_plug[0] / 2, adapter_back - 0.15 - adapter_plug[2]])
            cube([adapter_plug[1], adapter_plug[0], adapter_plug[2]]);
        // Wires out of the plug, bending back towards the back of the housing
        cols = ["red", "red", "black", "black", "orange", "black", "yellow", "green", "blue", "white"];
        if (show("wires")) for (i = [0 : 9])
            color(cols[i])
            {
                y = adapter_hc[1] + (i - 4.5) * 1.0;
                zc = adapter_back - 0.15 - adapter_plug[2] / 2;
                hull()
                {
                    translate([x0 - adapter_plug[1], y, zc]) sphere(d = 0.7, $fn = 8);
                    translate([x0 - adapter_plug[1] - 2, y, zc]) sphere(d = 0.7, $fn = 8);
                }
                hull()
                {
                    translate([x0 - adapter_plug[1] - 2, y, zc]) sphere(d = 0.7, $fn = 8);
                    translate([x0 - adapter_plug[1] - 3.5, y, zc - 6]) sphere(d = 0.7, $fn = 8);
                }
            }
    }
}

// What's cut from the back plate: the PCB's outline, through the plate, and
// a notch past its edge for the plug and the tool or fingers that push it in
module adapter_cut()
{
    translate([0, 0, plate_back - 1]) linear_extrude(plate_front - plate_back + 1.5) adapter_2d(grow = 0.4);
    translate([adapter_sock_front - adapter_plug_reach, adapter_hc[1] - adapter_conn[0] / 2 - 0.5, plate_back - 1])
        cube([adapter_plug_reach + 1, adapter_conn[0] + 1, min(adapter_back + 0.3, plate_front) - plate_back + 1]);
}
