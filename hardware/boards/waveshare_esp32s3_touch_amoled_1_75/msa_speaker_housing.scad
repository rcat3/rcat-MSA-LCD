// All-in-one housing for the Waveshare ESP32-S3-Touch-AMOLED-1.75: the
// display in the middle, one speaker in a pod below it, and a chamber behind
// for the battery. It screws into the voice port of an MSA Millennium mask.
//
// The board has its own amplifier, speaker socket and microphones, so there
// are no other boards to wire in: the speaker and the battery plug straight
// into the board.
//
// The parts:
//  - body: the display cup and the speaker pod, in one piece, hollow at the
//    back. The pod is shallow, so it stays in front of the mask's filter
//    mount, which is right below the voice port. The speaker is a sealed box
//    module. It goes into its pocket from the back of the pod, and the
//    adhesive frame on its front sticks it to the grille.
//  - chin: closes the back of the pod, and slopes from it back to the body,
//    over the filter mount. Four M2 x 8 socket head screws hold it on.
//  - back: closes the chamber. The battery is taped to it.
//  - collar, washer and gasket: the voice port thread, which turns freely on
//    the back, so the housing can be held level while only the collar is
//    screwed in. They're in hardware/masks/msa_millennium/turning_collar.scad.
//  - plungers: two pins that press the board's side buttons through the
//    thick wall on the left. Each goes in from inside the display's pocket
//    before the board, and a flange on its inner end keeps it from falling
//    out.
//
// The back is held on with four M2 x 8 socket head screws, which cut their
// own threads in the body.
//
// The board is turned a quarter turn from its usual way up, so its USB-C port
// faces right, out through the side of the housing, instead of into the pod.
// Its buttons and micro SD slot are on the left, and its pin header at the
// top. The firmware turns the picture to match.
//
// The board's dimensions are in board.scad, and the coordinates are the
// same: the front of the glass is at z = 0 and the body extends towards -z.
// The preview shows the parts of the mask around the voice port in red
// (hardware/masks/msa_millennium/keepout.scad), to check nothing hangs into
// them.
//
// Render a part to print with, for example:
//   openscad -D 'part="body"' -o body.stl msa_speaker_housing.scad

part = "preview";           // "preview", "body", "chin", "back", "collar", "washer", "gasket", "plungers",
                            // "plunger_test", "pod_test", "lens_fit" or "layout"

include <board.scad>
include <../../masks/msa_millennium/turning_collar.scad>
include <../../masks/msa_millennium/keepout.scad>

/* [Speaker: DWEII 3525 box module, 4 ohm 3 W] */
spk_box = [35, 25, 6.5];    // long side, short side, thickness with the adhesive frame
spk_corner = 2.0;           // the box's corners, seen from the front
spk_cone = [30, 20.5];      // the oval cone, inside the adhesive frame on the front
spk_clear = 0.3;
spk_back_gap = 2.0;         // behind the speaker, for its wire to run to the chamber

/* [Battery: 103450 LiPo, 10 x 34 x 50 mm, 2000 mAh] */
battery = [50.5, 34, 9.9];  // taped to the back
battery_xy = [0, 0];        // where its center is, seen from the front
battery_rot = 0;
battery_clear = 0.5;
battery_gap = 4.0;          // in front of the battery, for the plugs in the board's speaker and
                            // battery sockets, which go in from behind, and their wires. The
                            // microphones hear through the voice port across this gap too

/* [Layout] */
board_rot = 90;             // USB-C to the right, buttons and micro SD to the left
pod_angle = -90;            // where the pod sits around the display: -90 is straight down
pod_inset = 1.2;            // between the display's cup and the speaker pocket
pod_wall = 4.0;             // around the speaker pocket, thick enough at the sides for the chin's screws
pod_corner = 7.5;           // rounds the pod's corners, seen from the front
waist_r = 40;               // the curves joining the pod to the body
body_r = 34;                // the body around the display, big enough for the battery's corners
body_wall = 3.0;
edge_chamfer = 1.0;         // back edge of the body
slab_chamfer = 0.5;         // back edge of the pod, where it meets the chin
front_round = 4.0;          // rounds the front edge of the body
bezel_step = 1.0;           // the ring around the display stands this far proud of the face
bezel_chamfer = 0.5;        // on the outside of the ring
chin_foot = 1.5;            // the chin's sloped underside meets the back this far below the body

/* [Side buttons: plungers through the wall] */
plunger_d = 3.6;
plunger_clear = 0.2;        // around the plunger in its hole
plunger_flange = [5.6, 1.0];    // diameter, thickness. It's tapered on its outer side so it
                                // prints without support
plunger_tip = 3.0;          // diameter of the end that presses the button
nub_gap = 0.1;              // between the plunger and its button
plunger_proud = 1.8;        // how far it stands out of the housing
plunger_test = [1.3, 1.8, 2.3];     // how far the "plunger_test" plungers stand out, two of each

/* [Grille] */
grille_t = 1.6;
cone_gap = 0.8;             // a recess in the back of the grille, in front of the cone. The
                            // speaker's adhesive frame sticks to the flat ring around it
grille_slot = 1.6;          // the slots run across the short side of the speaker
grille_bar = 1.2;

/* [Back] */
cover_t = 3.0;
cover_lip = 1.0;            // the back sits in a rebate this wide around the chamber
cover_screws = [[17, 23], [-17, 23], [17, -23.5], [-17, -23.5]];  // just outside the thread, and
                            // above and below the battery
screw_pilot = 1.7;          // M2 machine screws cut their own thread in this
screw_clear = 2.3;
screw_head = [4.2, 2.1];    // counterbore for an M2 socket head (3.8 x 2.0): diameter, depth
boss_r = 3.2;
chin_screw_grip = 3.5;      // how much of the chin each of its screws clamps, under the head

/* [Collar: the rest of its settings are in turning_collar.scad] */
collar_r = 35;              // a little bigger than the body, so it can be turned from the sides

$fn = 96;

z_front = cup_top;                                          // the ring around the display
z_face = z_front - bezel_step;                              // the rest of the front
z_land = z_face - grille_t - cone_gap;                      // the front of the speaker
z_spk_back = z_land - spk_box[2];
z_pod_back = z_spk_back - spk_back_gap;                     // where the chin meets the pod
chamber_depth = battery[2] + battery_gap;
chamber_top = plate_back;
z_back = chamber_top - chamber_depth - cover_t;
z_grip = z_back - gasket_t - collar_t;                      // the back of the collar's grip, and
                                                            // the front of the voice port's rim
z_button = (button_z[0] + button_z[1]) / 2;
plunger_r = [button_reach_r + nub_gap, cup_ir + 0.3, body_r + plunger_proud];  // tip, flange, outer end

pocket = [spk_box[0], spk_box[1]] + [2, 2] * spk_clear;
pod_dist = cup_or + pod_inset + pocket[1] / 2;
pod_size = pocket + [2, 2] * pod_wall;
pod_reach = pod_dist + pod_size[1] / 2;                     // the pod's outer end, from the center
chin_h = z_pod_back - z_back;
chin_shift = pod_reach - body_r - chin_foot;                // how far the underside slopes in
// The chin's screws, in the pod's coordinates: two near the pod's outer end
// and two near the body, in the pod's walls either side of the pocket
chin_screws = [for (x = [6, -7.5], s = [-1, 1]) [x, s * (pocket[0] / 2 + pod_wall / 2)]];


// Places children at the pod, turned so +x points away from the display and
// the speaker's long side runs along y
module at_pod()
{
    rotate([0, 0, pod_angle]) translate([pod_dist, 0, 0]) children();
}

module pod_2d(grow = 0)
{
    rounded_rect([pod_size[1], pod_size[0]] + [2, 2] * grow, pod_corner + grow);
}

module battery_2d(grow = 0)
{
    translate(battery_xy) rotate(battery_rot)
        offset(r = 1 + grow) square([battery[0], battery[1]] + [1, 1] * (2 * battery_clear - 2), center = true);
}

// The outline of the body seen from the front: the display and the pod, with
// curves of radius waist_r filling the corners between them.
module outline_2d()
{
    offset(r = -waist_r) offset(delta = waist_r)
        union()
        {
            circle(r = body_r, $fn = 192);
            at_pod() pod_2d();
        }
}

module bosses_2d()
{
    for (s = cover_screws) translate(s) circle(r = boss_r, $fn = 32);
}

module chamber_2d()
{
    difference()
    {
        circle(r = body_r - body_wall, $fn = 192);
        bosses_2d();
    }
}

module rebate_2d()
{
    circle(r = body_r - body_wall + cover_lip, $fn = 192);
}

// The pod's outline, swept from the back of the body (z_back), where it's
// chin_shift closer to the center, out to the back of the pod (z_pod_back):
// the shape of the chin before it's trimmed to fit around the body
module chin_sweep(grow = 0)
{
    at_pod() translate([0, 0, z_back])
        multmatrix([[1, 0, chin_shift / chin_h, -chin_shift], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]])
            linear_extrude(chin_h) pod_2d(grow);
}

// The rest of this is in the pod's coordinates, with the speaker's long side
// along y.

module pocket_2d()
{
    rounded_rect([pocket[1], pocket[0]], spk_corner + spk_clear);
}

// Slots across the speaker, over its cone
module grille_slots_2d()
{
    pitch = grille_slot + grille_bar;
    n = floor((spk_cone[0] + grille_bar) / pitch);
    intersection()
    {
        rotate(90) offset(delta = -1) stadium(spk_cone);
        for (i = [0 : n - 1])
            translate([-spk_cone[1] / 2, (i - (n - 1) / 2) * pitch - grille_slot / 2])
                offset(r = grille_slot / 2 - 0.01) offset(delta = -(grille_slot / 2 - 0.01))
                    square([spk_cone[1], grille_slot]);
    }
}

// What's cut out of the pod: the pocket the speaker sits in, open at the
// back, and the grille in front of it. The inner end of the pocket overlaps
// the chamber, which is how the speaker's wire gets to the board.
module pod_cut()
{
    translate([0, 0, z_pod_back - 1]) linear_extrude(z_land - z_pod_back + 1) pocket_2d();
    translate([0, 0, z_land - 0.01]) linear_extrude(cone_gap + 0.02) rotate(90) stadium(spk_cone);
    translate([0, 0, z_face - grille_t - 0.01]) linear_extrude(grille_t + 1) grille_slots_2d();
}

module body()
{
    difference()
    {
        union()
        {
            // The round part behind the display, and the front slab the pod
            // is part of
            rounded_prism(z_back, z_face, edge_chamfer, front_round) circle(r = body_r, $fn = 192);
            rounded_prism(z_pod_back, z_face, slab_chamfer, front_round) outline_2d();
            // The ring around the display
            translate([0, 0, z_face - 0.01])
            {
                cylinder(r = cup_or, h = bezel_step - bezel_chamfer + 0.01, $fn = 128);
                translate([0, 0, bezel_step - bezel_chamfer])
                    cylinder(r1 = cup_or, r2 = cup_or - bezel_chamfer, h = bezel_chamfer + 0.01, $fn = 128);
            }
        }
        // The display's pocket, with a small chamfer on its front edge
        translate([0, 0, plate_front]) cylinder(r = cup_ir, h = 20, $fn = 128);
        translate([0, 0, z_front - 0.6]) cylinder(r1 = cup_ir, r2 = cup_ir + 0.6, h = 0.61, $fn = 128);
        rotate([0, 0, board_rot])
        {
            plate_holes();
            wall_openings();
        }
        at_buttons() plunger_cut();
        // The chamber, and the rebate the back sits in
        translate([0, 0, z_back - 1]) linear_extrude(chamber_top - z_back + 1) chamber_2d();
        translate([0, 0, z_back - 1]) linear_extrude(cover_t + 1) rebate_2d();
        at_pod() pod_cut();
        for (s = cover_screws)
            translate([s[0], s[1], z_back - 1]) cylinder(d = screw_pilot, h = cover_t + 9, $fn = 16);
        at_pod() for (s = chin_screws)
            translate([s[0], s[1], z_pod_back - 1]) cylinder(d = screw_pilot, h = 7, $fn = 16);
    }
}

// The pod's back and the slope under it, trimmed to the body's outline seen
// from the front, and clear of the round part of the body
module chin()
{
    difference()
    {
        intersection()
        {
            chin_sweep();
            translate([0, 0, z_back - 1]) linear_extrude(chin_h + 2) outline_2d();
        }
        translate([0, 0, z_back - 1]) cylinder(r = body_r + 0.2, h = chin_h + 2, $fn = 192);
        // Screws into the pod's walls, their heads deep in counterbores
        at_pod() for (s = chin_screws) translate([s[0], s[1], 0])
        {
            translate([0, 0, z_back - 1]) cylinder(d = screw_clear, h = chin_h + 2, $fn = 16);
            translate([0, 0, z_back - 1]) cylinder(d = screw_head[0], h = chin_h + 1 - chin_screw_grip, $fn = 32);
        }
    }
}

// Places children at each side button, with +z pointing out from the center
module at_buttons()
{
    rotate([0, 0, board_rot]) for (a = button_angles) rotate([0, 0, a]) translate([0, 0, z_button]) rotate([0, 90, 0]) children();
}

// A plunger, with z the distance from the center
module plunger_shape(grow = 0)
{
    f = plunger_flange + [2, 0] * grow;
    t = plunger_r[1] + f[1];                    // the flange's outer face
    translate([0, 0, plunger_r[0]]) cylinder(d = plunger_tip + 2 * grow, h = plunger_r[1] - plunger_r[0] + 0.01, $fn = 32);
    translate([0, 0, plunger_r[1]]) cylinder(d = f[0], h = f[1], $fn = 48);
    translate([0, 0, t - 0.01]) cylinder(d1 = f[0], d2 = plunger_d + 2 * grow, h = (f[0] - plunger_d) / 2 - grow + 0.01, $fn = 48);
    translate([0, 0, t]) cylinder(d = plunger_d + 2 * grow, h = plunger_r[2] - t, $fn = 32);
}

// A plunger that stands out of the housing by proud, which can differ from
// plunger_proud for test prints
module plunger(proud = plunger_proud)
{
    end = plunger_r[2] + proud - plunger_proud;
    // Printed standing on its outer end
    translate([0, 0, end]) rotate([180, 0, 0])
        intersection()
        {
            union()
            {
                plunger_shape();
                translate([0, 0, plunger_r[1]]) cylinder(d = plunger_d, h = max(end, plunger_r[2]) - plunger_r[1], $fn = 32);
            }
            cylinder(r = 10, h = end);
        }
}

// The hole for a plunger, and a recess in the wall of the display's pocket
// for its flange
module plunger_cut()
{
    g = plunger_clear;
    plunger_shape(g);
    translate([0, 0, cup_ir - 2]) cylinder(d = plunger_flange[0] + 2 * g, h = plunger_r[1] - cup_ir + 2.01, $fn = 48);
    translate([0, 0, plunger_r[2] - 1]) cylinder(d = plunger_d + 2 * g, h = 10, $fn = 32);
}

// A hole through the back for an M2 socket head screw, counterbored from
// the back face
module screw_cut()
{
    translate([0, 0, z_back - 1]) cylinder(d = screw_clear, h = cover_t + 2, $fn = 16);
    translate([0, 0, z_back - 0.01]) cylinder(d = screw_head[0], h = screw_head[1] + 0.01, $fn = 32);
}

// Inside the thread, below the battery, so the microphones hear through the
// voice port
module mic_opening_2d()
{
    intersection()
    {
        circle(r = 19.5, $fn = 96);
        translate([-20, -20]) square([40, battery_xy[1] - battery[1] / 2 - battery_clear + 20]);
    }
}

// The back, with its back face at z_back
module back()
{
    difference()
    {
        translate([0, 0, z_back]) linear_extrude(cover_t) offset(delta = -0.2) rebate_2d();
        translate([0, 0, z_back - 1]) linear_extrude(cover_t + 2) mic_opening_2d();
        for (s = cover_screws) translate(s) screw_cut();
    }
    collar_posts();
}

// The pod on its own, open at the back, to try the speaker's fit and how it
// sounds
module pod_test()
{
    difference()
    {
        rounded_prism(z_pod_back, z_face, slab_chamfer, front_round) pod_2d();
        pod_cut();
    }
}

// The front 3 mm of the body around the display, to check the glass fits
// before printing the lot
module lens_fit()
{
    intersection()
    {
        body();
        translate([0, 0, z_front - 3]) cylinder(r = cup_or + 4, h = 4);
    }
}

// The layout seen from the front, with the battery, cover screws and the
// board's sockets, microphones, buttons, USB-C port and micro SD slot marked
module layout()
{
    color("gray") difference()
    {
        outline_2d();
        chamber_2d();
    }
    color("orange", 0.5) battery_2d();
    color("cyan") mic_opening_2d();
    rotate(board_rot)
    {
        color("red") for (a = button_angles) rotate(a) translate([cup_ir, -1.5]) square([body_r - cup_ir, 3]);
        color("blue") translate([(sd_x[0] + sd_x[1]) / 2 - sd_opening[0] / 2, cup_ir]) square([sd_opening[0], body_r - cup_ir]);
        color("blue") translate([-usb_opening[0] / 2, -body_r]) square([usb_opening[0], body_r - cup_ir]);
        color("magenta") for (m = mic_xy) translate(m) circle(d = 2);
        color("green") for (b = [spk_conn_box, bat_conn_box]) translate(b[0]) square(box_size(b));
        color("yellow") translate(header_box[0]) square(box_size(header_box));
    }
    color("purple")
    {
        for (c = cover_screws) translate(c) circle(d = screw_head[0]);
        at_pod() for (s = chin_screws) translate(s) circle(d = screw_head[0]);
    }
    color("black", 0.6) at_pod() pocket_2d();
    color("dimgray", 0.4) circle(r = cup_ir);
}

if (part == "preview")
{
    color("dimgray") body();
    color("gray") chin();
    color("gray") back();
    color("dimgray") collar();
    color("silver") washer();
    color("midnightblue") translate([0, 0, -lens_t]) cylinder(d = lens_d, h = lens_t);
    color("red", 0.4) translate([0, 0, z_grip]) mask_keepout();
}
else if (part == "layout") layout();
// Printed back down
else if (part == "body") translate([0, 0, -z_back]) body();
// Printed on the face that meets the pod, with the slope facing up
else if (part == "chin") rotate([180, 0, 0]) translate([0, 0, -z_pod_back]) chin();
else if (part == "plungers") for (i = [0, 1]) translate([i * 8, 0, 0]) plunger();
else if (part == "plunger_test")
    for (i = [0 : len(plunger_test) - 1], j = [0, 1]) translate([i * 8, j * 8, 0]) plunger(plunger_test[i]);
else if (part == "collar") collar_print();
else if (part == "washer") washer_print();
else if (part == "gasket") gasket();
else if (part == "lens_fit") rotate([180, 0, 0]) translate([0, 0, -z_front]) lens_fit();
else if (part == "pod_test") translate([0, 0, -z_pod_back]) pod_test();
else if (part == "back") rotate([180, 0, 0]) translate([0, 0, -z_back - cover_t]) back();
