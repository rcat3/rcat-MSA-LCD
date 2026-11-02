// All-in-one housing for the Waveshare ESP32-S3-Touch-LCD-1.46: the display
// in the middle, a small speaker on each side, and a chamber behind for the
// battery, the two MAX98357A amplifier boards and a USB-C charging port. It
// screws into the voice port of an MSA Millennium mask.
//
// The parts:
//  - body: the display cup and the speaker pods, in one piece, hollow at the
//    back. The speakers are sealed box modules. Each goes in from the back
//    into a pocket behind a grille in the front face, and the adhesive frame
//    on its front sticks it to the grille.
//  - back: closes the chamber. The battery is taped to it, label side down with its leads at the lower
//    left (seen from the front, where there's room for them to turn), and
//    an amplifier is screwed to it behind each speaker.
//  - collar: carries the voice port thread and turns freely on the back, so
//    the housing can be held level while only the collar is screwed in. It
//    goes over four posts on the back, which end flush with the back of its
//    grip, inside the bore, and the washer is screwed to their ends behind
//    it, so it can't come off.
//  - washer: holds the collar on, with four M2 x 6 socket head screws. It's
//    in two halves, which go in through the threaded end of the collar and
//    sit in a pocket just behind the grip.
//  - gasket: a thin ring between the back and the collar, so the housing
//    doesn't turn on its own once it's screwed in. Print it in TPU, or cut
//    it from 1 mm rubber or craft foam.
//  - plungers: two pins that press the board's side buttons through the
//    thick wall at the top. Each goes in from inside the display's pocket
//    before the board, and a flange on its inner end keeps it from falling
//    out.
//  - spacers: six short tubes the amplifiers and the charging port's board
//    stand on. M2 x 10 screws go through the back from outside, their heads
//    in counterbores, then through a spacer and the board, with a nut on top.
//
// The back is held on with four M2 x 8 socket head screws, which cut their
// own threads in the body.
//
// The board's own USB-C port faces into a pod, so the housing has a charging
// port of its own in the lower end of the right pod: a USB-C breakout (with
// 5.1k resistors from CC1 and CC2 to GND, so chargers turn on) wired to
// USB_5V and GND on the board's pin header, which feed its charger. It sits
// behind the speaker. Don't plug into both ports at once.
//
// The board is turned a quarter turn from its usual way up, so its buttons
// are at the top, clear of the pods. The firmware turns the picture to match.
//
// The board's dimensions and the thread are in msa_1_46_board.scad, and the
// coordinates are the same: the front of the glass is at z = 0 and the body
// extends towards -z.
//
// Render a part to print with, for example:
//   openscad -D 'part="body"' -o body.stl msa_1_46_speaker_housing.scad

part = "preview";           // "preview", "body", "back", "collar", "washer", "gasket", "plungers", "plunger_test",
                            // "spacers", "pod_test" or "layout"

include <msa_1_46_board.scad>

// The ribbon cable on the board's pin header leaves its plug towards the
// bottom, so it comes out below the battery. Its wires are spliced in the
// space there, between the lower cover screws.
header_plug_side = -1;
header_plug_reach = 4.1;    // from the ends of the board's standoffs to the back of the plug,
                            // with the ribbon on it

/* [Speakers: DWEII 3525 box modules, 4 ohm 3 W] */
spk_box = [35, 25, 6.5];    // long side, short side, thickness with the adhesive frame
spk_corner = 2.0;           // the box's corners, seen from the front
spk_cone = [30, 20.5];      // the oval cone, inside the adhesive frame on the front
spk_wire_notch = [14, 2.5]; // width, depth of a notch in the lower end of the pocket, for the
                            // wire where it leaves the speaker
spk_clear = 0.3;

/* [Battery: 103450 LiPo, 10 x 34 x 50 mm, 2000 mAh] */
battery = [50.5, 34, 9.9];  // taped to the back
battery_xy = [0, 0];        // where its center is, seen from the front. The board's microphone
                            // is just inside its lower edge
battery_rot = 0;
battery_clear = 0.5;
mic_gap = 1.5;              // in front of the battery, so the microphone hears through the
                            // opening in the back below it. More if the ribbon cable's plug
                            // needs it

/* [Amplifiers: MAX98357A breakouts, one behind each speaker] */
amp_size = [18.8, 17.55];   // across the pod, along it
amp_holes = [12.2, 2.5];    // spacing, distance from the edge. They're along the edge furthest
                            // from the display
amp_y = [0, 2.5];           // where each sits along its pod, left and right. The right one is
                            // above the charging port

/* [Charging port: generic USB-C breakout, 0.5 x 0.85 inch] */
usb_board = [21.6, 12.6, 1.6];  // across, front to back, thickness
usb_holes = [16.0, 2.35];   // spacing, distance from the front edge
usb_socket = [9.0, 3.2, 1.5];   // width, height, and how far it sticks out past the front edge
usb_clear = 0.3;

/* [Spacers the boards stand on, clear of the solder joints underneath] */
spacer_h = 3.5;
spacer_d = 4.5;

/* [Layout] */
board_rot = 90;             // buttons up, micro SD down
pod_angle = -12;            // where the pods sit around the display: 0 is straight out to the
                            // sides, negative tips them down
pod_inset = 1.2;            // between the display's cup and each speaker pocket
pod_wall = 3.0;             // around each speaker pocket
pod_corner = 7.5;           // rounds the pods' corners, seen from the front. Any rounder and the
                            // pockets' corners would break through
waist_r = 40;               // the curves joining everything together
body_r = 33;                // the body around the display, big enough for the cover screws
                            // between the thread and the outside
body_wall = 3.0;
edge_chamfer = 1.0;         // back edge of the body
front_round = 4.0;          // rounds the front edge of the body
bezel_step = 1.0;           // the ring around the display stands this far proud of the face
bezel_chamfer = 0.5;        // on the outside of the ring

/* [Side buttons: plungers through the wall] */
plunger_d = 3.6;
plunger_clear = 0.2;        // around the plunger in its hole
plunger_flange = [5.6, 1.0];    // diameter, thickness. It's tapered on its outer side so it
                                // prints without support
plunger_tip = 3.0;          // diameter of the end that presses the button
plunger_proud = 1.8;        // how far it stands out of the housing. With 0.8 one of the buttons
                            // only clicked once its plunger was pushed into the hole
plunger_test = [1.3, 1.8, 2.3];     // how far the "plunger_test" plungers stand out, two of each

/* [Grilles] */
grille_t = 1.6;
cone_gap = 0.8;             // a recess in the back of the grille, in front of the cone. The
                            // speaker's adhesive frame sticks to the flat ring around it
grille_slot = 1.6;          // the slots run across the short side of each speaker, so the bars
grille_bar = 1.2;           // between them bridge when printing

/* [Back] */
cover_t = 3.0;
cover_lip = 1.0;            // the back sits in a rebate this wide around the chamber
cover_screws = [[17, 23], [-17, 23], [17, -23.5], [-17, -23.5]];  // just outside the thread, and
                            // above and below the battery
screw_pilot = 1.7;          // M2 machine screws cut their own thread in this
screw_clear = 2.3;
screw_head = [4.2, 2.1];    // counterbore for an M2 socket head (3.8 x 2.0): diameter, depth
boss_r = 3.2;

/* [Collar] */
collar_r = 34;              // the knurled grip, a little bigger than the body around the display
                            // so it can be turned from above and below
collar_t = 3.5;             // the grip, which is held between the gasket and the washer
collar_bore = 20.3;         // the grip's bore, around the posts
collar_clear = 0.3;
gasket_t = 0.5;             // the gap the gasket is squeezed into
gasket = [20.8, 33.0, 1.0]; // inner and outer radius, thickness
post_r = 17.7;              // the posts on the back are this far from the center
post_d = 4.6;               // thick enough that the washer's screws don't split them
post_angles = [45, 135, 225, 315];  // clear of the opening for the microphone at the bottom
washer = [15.0, 22.2, 2.0]; // inner and outer radius, thickness
washer_arc = 130;           // each half spans this many degrees, so it fits through the thread

$fn = 96;

z_front = cup_top;                                          // the ring around the display
z_face = z_front - bezel_step;                              // the rest of the front
z_land = z_face - grille_t - cone_gap;                     // the front of each speaker
z_spk_back = z_land - spk_box[2];
battery_gap = max(mic_gap, header_plug_reach - plate_t + 1.0);
chamber_depth = battery[2] + battery_gap;
chamber_top = plate_back;
z_back = chamber_top - chamber_depth - cover_t;
z_grip = z_back - gasket_t - collar_t;                      // the back of the collar's grip
z_button = (button_z[0] + button_z[1]) / 2;
plunger_r = [button_reach_r + nub_gap, cup_ir + 0.3, body_r + plunger_proud];  // tip, flange, outer end
z_usb = z_back + cover_t + spacer_h + usb_board[2] + usb_socket[1] / 2;     // the socket's center

pocket = [spk_box[0], spk_box[1]] + [2, 2] * spk_clear;
pod_dist = cup_or + pod_inset + pocket[1] / 2;
pod_size = pocket + [2, 2] * pod_wall + [spk_wire_notch[1], 0];


// Places children at each pod, turned so +x points away from the display and
// the speaker's long side runs along y, with -y at the bottom. The left pod
// is a mirror image of the right one.
module at_pods()
{
    for (m = [0, 1]) mirror([m, 0, 0]) rotate([0, 0, pod_angle]) translate([pod_dist, 0, 0]) children();
}

// Places children at the front edge of the charging port's board, in the
// lower end of the right pod, with +y pointing into the pod
module at_usb()
{
    rotate([0, 0, pod_angle]) translate([pod_dist, -pocket[0] / 2 - spk_wire_notch[1], 0]) children();
}

// Places children at the screw holes of the charging port's board and the
// amplifiers
module at_board_holes()
{
    at_usb() for (s = [-1, 1]) translate([s * usb_holes[0] / 2, usb_holes[1], 0]) children();
    for (m = [0, 1])
        mirror([1 - m, 0, 0]) rotate([0, 0, pod_angle])
            translate([pod_dist + amp_size[0] / 2 - amp_holes[1], amp_y[m], 0])
                for (s = [-1, 1]) translate([0, s * amp_holes[0] / 2, 0]) children();
}

// A pod seen from the front, longer at the bottom for the wire notch
module pod_2d()
{
    translate([0, -spk_wire_notch[1] / 2]) rounded_rect([pod_size[1], pod_size[0]], pod_corner);
}

module rounded_rect(size, r)
{
    offset(r = r) square(size - [2, 2] * r, center = true);
}

module stadium(size)
{
    hull()
        for (s = [-1, 1])
            translate([s * (size[0] - size[1]) / 2, 0]) circle(d = size[1], $fn = 64);
}

module battery_2d(grow = 0)
{
    translate(battery_xy) rotate(battery_rot)
        offset(r = 1 + grow) square([battery[0], battery[1]] + [1, 1] * (2 * battery_clear - 2), center = true);
}

// The outline of the body seen from the front: the display and the pods,
// with curves of radius waist_r filling the corners between them.
module outline_2d()
{
    offset(r = -waist_r) offset(delta = waist_r)
        union()
        {
            circle(r = body_r, $fn = 192);
            at_pods() pod_2d();
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
        offset(delta = -body_wall) outline_2d();
        bosses_2d();
    }
}

module rebate_2d()
{
    offset(delta = cover_lip) offset(delta = -body_wall) outline_2d();
}

// Extrudes a 2D outline from z0 to z1 with its back edge chamfered by c and
// its front edge rounded with radius r, in steps.
module rounded_prism(z0, z1, c, r)
{
    steps = 8;
    s = r / steps;
    chamfered_prism(z0, z1 - r + 0.01, c, front = false) children();
    for (i = [0 : steps - 1])
    {
        d = r - i * s;                  // how far below the front the slice ends
        translate([0, 0, z1 - d]) linear_extrude(s + (i < steps - 1 ? 0.01 : 0))
            offset(delta = -(r - sqrt(r * r - pow(r - (d - s), 2)))) children();
    }
}

// Extrudes a 2D outline from z0 to z1 with its edges chamfered by c.
module chamfered_prism(z0, z1, c, front = true)
{
    steps = 4;
    s = c / steps;
    translate([0, 0, z0 + c]) linear_extrude(z1 - z0 - 2 * c) children();
    for (i = [0 : steps - 1])
    {
        inset = c - i * s;
        translate([0, 0, z0 + i * s]) linear_extrude(s + 0.01) offset(delta = -inset) children();
        if (front)
            translate([0, 0, z1 - (i + 1) * s - 0.01]) linear_extrude(s + 0.01) offset(delta = -inset) children();
    }
    if (!front) translate([0, 0, z1 - c]) linear_extrude(c) children();
}

// The rest of this is in a pod's coordinates, with the speaker's long side
// along y.

module pocket_2d()
{
    rounded_rect([pocket[1], pocket[0]], spk_corner + spk_clear);
    translate([-spk_wire_notch[0] / 2, -pocket[0] / 2 - spk_wire_notch[1]])
        square([spk_wire_notch[0], pocket[0] / 2 + spk_wire_notch[1]]);
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

// What's cut out of each pod: the pocket the speaker sits in, open to the
// chamber behind, and the grille in front of it
module pod_cut()
{
    translate([0, 0, z_back - 1]) linear_extrude(z_land - z_back + 1) pocket_2d();
    translate([0, 0, z_land - 0.01]) linear_extrude(cone_gap + 0.02) rotate(90) stadium(spk_cone);
    translate([0, 0, z_face - grille_t - 0.01]) linear_extrude(grille_t + 1) grille_slots_2d();
}

module body()
{
    difference()
    {
        union()
        {
            difference()
            {
                union()
                {
                    rounded_prism(z_back, z_face, edge_chamfer, front_round) outline_2d();
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
                at_pods() pod_cut();
                at_usb() usb_cut();
            }
        }
        for (s = cover_screws)
            translate([s[0], s[1], z_back - 1]) cylinder(d = screw_pilot, h = cover_t + 9, $fn = 16);
    }
}

// The charging port's board sits in a slot in the end of the pod, so the
// front of its socket is close to the outside. The slot only goes as far
// forward as the board, so the speaker still sits on the end of its pocket.
// The opening around the socket is big enough for a plug's overmold.
module usb_cut()
{
    translate([-usb_board[0] / 2 - usb_clear, -usb_clear, z_back + cover_t - 0.01])
        cube([usb_board[0] + 2 * usb_clear, usb_board[1] + usb_clear, z_usb + usb_socket[1] + 1 - z_back - cover_t]);
    translate([0, 0, z_usb])
        rotate([90, 0, 0])
            hull()
                for (s = [-1, 1])
                    translate([s * (usb_opening[0] - usb_opening[1]) / 2, 0, -1])
                        cylinder(d = usb_opening[1], h = pod_wall + 4, $fn = 48);
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

module spacers()
{
    for (i = [0 : 5])
        translate([(i % 3) * (spacer_d + 3), floor(i / 3) * (spacer_d + 3), 0])
            difference()
            {
                cylinder(d = spacer_d, h = spacer_h, $fn = 32);
                translate([0, 0, -1]) cylinder(d = screw_clear, h = spacer_h + 2, $fn = 16);
            }
}

// Inside the thread, below the battery
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
        // So the microphone can hear through the voice port
        translate([0, 0, z_back - 1]) linear_extrude(cover_t + 2) mic_opening_2d();
        for (s = cover_screws) translate(s) screw_cut();
        at_board_holes() screw_cut();
    }
    // The posts the collar turns on, with pilot holes for the washer's screws
    for (a = post_angles)
        rotate([0, 0, a]) translate([post_r, 0, z_grip])
            difference()
            {
                cylinder(d = post_d, h = z_back - z_grip + 0.01, $fn = 32);
                translate([0, 0, -0.01]) cylinder(d = screw_pilot, h = 5, $fn = 16);
            }
}

module at_posts()
{
    for (a = post_angles) rotate([0, 0, a]) translate([post_r, 0, 0]) children();
}

// The thread, with a knurled grip on its front end. Behind the grip, the
// bore is wider for the washer and its screw heads.
module collar()
{
    pocket_r = washer[1] + collar_clear;
    pocket_d = washer[2] + 0.3;
    bore_r = 21 * thread_scale;
    difference()
    {
        union()
        {
            translate([0, 0, z_grip])
                linear_extrude(collar_t)
                    difference()
                    {
                        circle(r = collar_r, $fn = 192);
                        for (i = [0 : 47]) rotate(i * 360 / 48) translate([collar_r + 0.4, 0]) circle(r = 1.2, $fn = 16);
                    }
            translate([0, 0, z_grip + 0.1]) rotate([180, 0, 0]) vpu_thread(thread_scale);
        }
        translate([0, 0, z_grip - 10]) cylinder(r = collar_bore, h = collar_t + 11, $fn = 128);
        translate([0, 0, z_grip - pocket_d]) cylinder(r = pocket_r, h = pocket_d + 0.01, $fn = 128);
        // Sloped, so it prints without support
        translate([0, 0, z_grip - pocket_d - (pocket_r - bore_r) + 0.01])
            cylinder(r1 = bore_r - 0.01, r2 = pocket_r, h = pocket_r - bore_r, $fn = 128);
    }
}

module washer()
{
    translate([0, 0, z_grip - washer[2]]) for (a = [90, 270]) rotate([0, 0, a]) washer_half();
}

// Half of the washer, centered on +x, with holes for the two posts either
// side of it
module washer_half()
{
    difference()
    {
        intersection()
        {
            cylinder(r = washer[1], h = washer[2], $fn = 128);
            linear_extrude(washer[2])
                polygon(concat([[0, 0]], [for (t = [-washer_arc / 2 : 5 : washer_arc / 2]) 2 * washer[1] * [cos(t), sin(t)]]));
        }
        translate([0, 0, -1]) cylinder(r = washer[0], h = washer[2] + 2, $fn = 128);
        for (a = post_angles)
            if (abs(a - 90) < washer_arc / 2)
                rotate([0, 0, a - 90]) translate([post_r, 0, -1]) cylinder(d = screw_clear, h = washer[2] + 2, $fn = 16);
    }
}

module gasket()
{
    difference()
    {
        cylinder(r = gasket[1], h = gasket[2], $fn = 192);
        translate([0, 0, -1]) cylinder(r = gasket[0], h = gasket[2] + 2, $fn = 192);
    }
}

// One pod on its own, to try the speaker's fit and how it sounds
module pod_test()
{
    difference()
    {
        rounded_prism(z_spk_back - 2, z_face, edge_chamfer, front_round) pod_2d();
        pod_cut();
    }
}

// The layout seen from the front, with the battery, amplifiers, cover
// screws and the board's buttons, microphone and micro SD slot marked
module layout()
{
    color("gray") difference()
    {
        outline_2d();
        chamber_2d();
    }
    color("orange", 0.5) battery_2d();
    for (m = [0, 1])
        color("green", 0.6) mirror([1 - m, 0]) rotate(pod_angle) translate([pod_dist, amp_y[m]]) square(amp_size, center = true);
    color("blue", 0.6) at_usb()
    {
        translate([-usb_board[0] / 2, 0]) square([usb_board[0], usb_board[1]]);
        translate([-usb_socket[0] / 2, -usb_socket[2]]) square([usb_socket[0], usb_socket[2]]);
    }
    color("cyan") mic_opening_2d();
    rotate(board_rot)
    {
        color("red") for (a = button_angles) rotate(a) translate([cup_ir, -button_tab / 2]) square([body_r - cup_ir, button_tab]);
        color("blue") translate([-body_r, -sd_opening[0] / 2]) square([body_r - cup_ir, sd_opening[0]]);
        color("magenta") translate(mic_xy) circle(d = 2);
    }
    color("purple")
    {
        for (c = cover_screws) translate(c) circle(d = screw_head[0]);
        at_board_holes() circle(d = screw_head[0]);
    }
    color("black", 0.6) at_pods() pocket_2d();
    color("dimgray", 0.4) circle(r = cup_ir);
}

if (part == "preview")
{
    color("dimgray") body();
    color("gray") back();
    color("dimgray") collar();
    color("silver") washer();
    color("midnightblue") translate([0, 0, -2]) cylinder(d = lens_d, h = 2);
}
else if (part == "layout") layout();
// Printed back down
else if (part == "body") translate([0, 0, -z_back]) body();
else if (part == "spacers") spacers();
else if (part == "plungers") for (i = [0, 1]) translate([i * 8, 0, 0]) plunger();
else if (part == "plunger_test")
    for (i = [0 : len(plunger_test) - 1], j = [0, 1]) translate([i * 8, j * 8, 0]) plunger(plunger_test[i]);
else if (part == "collar") rotate([180, 0, 0]) translate([0, 0, -z_grip - collar_t]) collar();
else if (part == "washer") for (i = [0, 1]) translate([0, i * 18, 0]) rotate([0, 0, 90]) washer_half();
else if (part == "gasket") gasket();
else if (part == "pod_test") translate([0, 0, -z_spk_back + 2]) pod_test();
else if (part == "back") rotate([180, 0, 0]) translate([0, 0, -z_back - cover_t]) back();
