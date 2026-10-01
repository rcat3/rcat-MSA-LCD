// Housing that screws the Waveshare ESP32-S3-Touch-LCD-1.46 (the version
// with the cover glass) into the voice port of an MSA Millennium mask.
//
// The board sits in a cup outside the mask with its glass flush with the
// front. The cup's back plate carries the MSA thread, and the space inside
// the thread holds the battery and power switch, like the 1.28" design this
// is based on (see README.md for credits and license).
//
// Coordinates follow Waveshare's STEP model of the board: the front of the
// glass is at z = 0, the board extends towards -z, and x/y are as seen from
// the front with the USB-C port at the bottom (-y).
//
// Render the part to print with:  openscad -o housing.stl msa_1_46_housing.scad
// Set `part` to "lens_fit" or "thread_test" for the quick test prints.

part = "housing";           // "housing", "fit_test", "lens_fit" or "thread_test"
thread_scale = 0.997;       // scales the thread's diameter. From the test rings: 99.7% fits best
                            // (98% is too loose, 102% doesn't fit)
thread_lead_in = [1.5, 1.2];  // bevel on the start of the thread (length, depth) to help it catch

/* [Board: Waveshare ESP32-S3-Touch-LCD-1.46 with cover] */
lens_d = 44.77;                     // cover glass
standoffs = [[12.00, -14.95], [-11.54, -15.45], [0.00, 17.75]];   // M2 threaded standoffs on the back
standoff_back_z = -10.91;           // the back of the standoffs, where the board rests on the back plate
header_box = [[-17.02, -6.10], [-11.52, 6.33]];     // 2x10 pin header, pins reach z = -12.95
battery_conn_box = [[3.67, 12.00], [11.32, 17.20]];
mic_xy = [-14.20, -12.05];
speaker_box = [[3.85, -5.00], [18.50, 5.00]];
usb_z = [-10.64, -7.48];            // USB-C port, centered on x = 0 at the bottom edge, front at y = -23.0
// The bottom of the board is flattened and sticks out past the round glass.
// Its outline (half of it, mirrored) from z = -10.64 to the glass at -2.1:
bottom_outline = [[0, -21.90], [7, -21.90], [8, -21.68], [9, -21.28], [10, -20.61], [11, -19.61]];
bottom_outline_z = [-10.64, -2.10];
display_tab = [[-7.02, -23.12], [6.20, -21.0]];     // the display's flex tab sticks out further still
display_tab_z = [-3.80, -2.07];
sd_y = [-9.00, 7.10];               // micro SD socket, card goes in from the left edge (-x)
sd_z = [-6.80, -4.35];
sd_shift = [1.0, 1.0];              // the real card sits this much further towards +y and the front
                                    // than the 3D model shows (from the first print)
button_angles = [34.7, -34.7];      // side buttons (PWR and BOOT) on the right edge
button_z = [-9.20, -7.00];
button_reach_r = 21.0;              // how far out the button actuators reach

/* [Housing] */
bottom_margin = 0.3;                // around the flat bottom and the display tab
clearance = 0.2;                    // around the glass (0.35 was looser than needed)
wall = 2.0;
rim_above_glass = 0.3;              // the rim stands this far proud of the glass to protect it
plate_t = 2.0;                      // back plate the board screws to
screw_d = 2.2;                      // M2 clearance, snug so the screws center the board
screw_head_d = 4.0;                 // M2 countersunk head
usb_opening = [12.4, 6.6];          // width, height of the slot for the USB-C plug. The port sits
                                    // 1.6 mm behind the outside of the wall, so the plug's
                                    // overmold has to fit into the slot to plug in all the way
sd_opening = [14.0, 3.4];           // width, height of the slot for the micro SD card
button_tab = 5.0;                   // width of the flexible tabs that press the side buttons
tab_gap = 0.6;
tab_t = 1.0;                        // tabs are thinner than the wall so they flex
nub_gap = 0.1;                      // between the nubs and the buttons

$fn = 128;

cup_ir = lens_d / 2 + clearance;
cup_or = cup_ir + wall;
cup_top = rim_above_glass;
plate_front = standoff_back_z;
plate_back = plate_front - plate_t;

function box_center(b) = [(b[0][0] + b[1][0]) / 2, (b[0][1] + b[1][1]) / 2];
function box_size(b) = [b[1][0] - b[0][0], b[1][1] - b[0][1]];

module slab(b, z0, z1, grow = 0)
{
    translate([b[0][0] - grow, b[0][1] - grow, z0])
        cube([box_size(b)[0] + 2 * grow, box_size(b)[1] + 2 * grow, z1 - z0]);
}

// The MSA thread from the 1.28" design. vendor/vpu_thread.stl has had the
// original's inner lip removed so the inside is a plain 42 mm bore. Its front
// face is at z = 0 and it screws into the mask from the z = 8 end, which gets
// a bevel so the thread catches more easily.
module vpu_thread(s)
{
    thread_r = 26 * s;
    difference()
    {
        scale([s, s, 1]) import("vendor/vpu_thread.stl", convexity = 10);
        translate([0, 0, 8 - thread_lead_in[0]])
            difference()
            {
                cylinder(r = thread_r + 2, h = thread_lead_in[0] + 1);
                translate([0, 0, -0.01])
                    cylinder(r1 = thread_r + 0.01, r2 = thread_r - thread_lead_in[1], h = thread_lead_in[0] + 0.02);
            }
    }
}

// The thread on the back of the housing, front face against the back plate.
// It's turned over with a rotation, not a mirror, which would reverse it.
// It overlaps the plate slightly so the two fuse into one solid.
module thread()
{
    translate([0, 0, plate_back + 0.1]) rotate([180, 0, 0]) vpu_thread(thread_scale);
}

module cup()
{
    difference()
    {
        translate([0, 0, plate_back]) cylinder(r = cup_or, h = cup_top - plate_back);
        // Pocket for the board, open at the front
        translate([0, 0, plate_front]) cylinder(r = cup_ir, h = 20);
        // Small chamfer on the front edge
        translate([0, 0, cup_top - 0.6]) cylinder(r1 = cup_ir, r2 = cup_ir + 0.6, h = 0.61);
    }
}

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
    // Pin header, battery plug and wires, microphone, speaker
    slab(header_box, plate_back - 1, plate_front + 1, grow = 0.5);
    slab(battery_conn_box, plate_back - 1, plate_front + 1, grow = 1.5);
    translate([mic_xy[0], mic_xy[1], plate_back - 1]) cylinder(d = 2.0, h = plate_t + 2, $fn = 24);
    c = box_center(speaker_box);
    for (dx = [-5 : 2.5 : 5], dy = [-2.5 : 2.5 : 2.5])
        translate([c[0] + dx, c[1] + dy, plate_back - 1]) cylinder(d = 1.6, h = plate_t + 2, $fn = 16);
}

// Openings in the side wall
module wall_openings()
{
    // Pocket inside the bottom of the wall for the flattened bottom of the
    // board. It stops short of the front so the rim stays whole.
    pocket_top = display_tab_z[1] + 0.5;
    half = concat(bottom_outline, [[bottom_outline[len(bottom_outline) - 1][0], -15]]);
    outline = concat(half, [for (i = [len(half) - 1 : -1 : 0]) [-half[i][0], half[i][1]]]);
    translate([0, 0, plate_front])
        linear_extrude(pocket_top - plate_front) offset(delta = bottom_margin) polygon(outline);
    // The display's tab, which reaches almost to the outside of the wall
    translate([0, 0, display_tab_z[0] - bottom_margin])
        slab(display_tab, 0, pocket_top - display_tab_z[0] + bottom_margin, grow = bottom_margin);
    // USB-C at the bottom: a rounded slot around the port
    translate([0, -cup_or + wall / 2, (usb_z[0] + usb_z[1]) / 2])
        rotate([90, 0, 0])
            hull()
                for (s = [-1, 1])
                    translate([s * (usb_opening[0] - usb_opening[1]) / 2, 0, 0])
                        cylinder(d = usb_opening[1], h = wall + 4, center = true, $fn = 48);
    // Micro SD slot on the left
    translate([-cup_or - 1, (sd_y[0] + sd_y[1]) / 2 + sd_shift[0] - sd_opening[0] / 2,
               (sd_z[0] + sd_z[1]) / 2 + sd_shift[1] - sd_opening[1] / 2])
        cube([wall + 3, sd_opening[0], sd_opening[1]]);
}

// A flexible tab in the wall over each side button, hinged at the front, with
// a nub on the inside that presses the button. The free end is at the back,
// just clear of the back plate, which makes the tab as long as it can be.
tab_front = display_tab_z[1] + 0.5;
tab_back = plate_front + tab_gap;

module button_tab_cut(angle)
{
    rotate([0, 0, angle])
    {
        // U-shaped slot around the tab
        difference()
        {
            translate([cup_ir - 1, -button_tab / 2 - tab_gap, tab_back - tab_gap])
                cube([wall + 2, button_tab + 2 * tab_gap, tab_front - tab_back + tab_gap]);
            translate([cup_ir - 1, -button_tab / 2, tab_back])
                cube([wall + 2, button_tab, tab_front - tab_back + 1]);
        }
        // Thin the tab from the outside so it flexes
        translate([cup_ir + tab_t, -button_tab / 2 - 0.01, tab_back - 0.01])
            cube([wall, button_tab + 0.02, tab_front - tab_back + 0.02]);
    }
}

module button_nub(angle)
{
    r0 = button_reach_r + nub_gap;
    rotate([0, 0, angle])
        translate([r0, -1.5, button_z[0] + 0.3])
            cube([cup_ir - r0 + 0.01, 3, button_z[1] - button_z[0] - 0.6]);
}

module housing(with_thread = true)
{
    difference()
    {
        union()
        {
            cup();
            if (with_thread) thread();
        }
        plate_holes();
        wall_openings();
        for (a = button_angles) button_tab_cut(a);
    }
    for (a = button_angles) button_nub(a);
}

// The front 3 mm of the cup, to check the glass fits before printing the lot.
// A flange around the outside keeps the thin ring from going oval.
module lens_fit()
{
    intersection()
    {
        cup();
        translate([0, 0, cup_top - 3]) cylinder(r = cup_or + 1, h = 4);
    }
    translate([0, 0, cup_top - 3])
        difference()
        {
            cylinder(r = cup_or + 4, h = 1.5);
            translate([0, 0, -1]) cylinder(r = cup_or - 0.01, h = 4);
        }
}

// Thin thread rings at a few scales, to find the best fit in the mask. Each
// has a bar across it to grip, so it can be screwed all the way in and out.
module thread_test()
{
    scales = [0.994, 0.997, 1.000];
    for (i = [0 : len(scales) - 1])
        translate([i * 56, 0, 0])
        {
            vpu_thread(scales[i]);
            translate([-21.5, -2.5, 0]) cube([43, 5, 6]);
            // Notches on the rim say which scale it is: 1 to 3
            for (n = [0 : i])
                rotate([0, 0, n * 8]) translate([0, 21.2, 7.2]) cube([1.2, 1.5, 1.6]);
        }
}

// Flip so the front is on the print bed
if (part == "housing") rotate([180, 0, 0]) housing();
else if (part == "fit_test") rotate([180, 0, 0]) housing(with_thread = false);
else if (part == "lens_fit") rotate([180, 0, 0]) translate([0, 0, -cup_top]) lens_fit();
else if (part == "thread_test") thread_test();
