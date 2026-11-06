// A collar that carries the voice port thread and turns freely on the back
// of a housing, so the housing can be held level while only the collar is
// screwed into the mask. Include this file after voice_port.scad.
//
// The collar goes over four posts on the housing's back, which end flush with
// the back of its grip, inside the bore. A washer in two halves goes in
// through the threaded end and is screwed to the ends of the posts, so the
// collar can't come off. A thin gasket between the back and the collar stops
// the housing turning on its own once it's screwed in.
//
// The housing that includes this file sets:
//   z_back       the back face of the housing, where the posts start
//   z_grip       the back of the collar's grip, where the posts end. The
//                thread starts here, so this is where the front of the
//                voice port's rim ends up.
//   screw_pilot  hole M2 screws cut their own thread in
//   screw_clear  M2 clearance hole

collar_r = 34;              // the knurled grip, a little bigger than the body around the display
                            // so it can be turned from above and below
collar_t = 3.5;             // the grip, which is held between the gasket and the washer
collar_bore = 20.3;         // the grip's bore, around the posts
collar_clear = 0.3;
gasket_t = 0.5;             // the gap the gasket is squeezed into
gasket = [20.8, 33.0, 1.0]; // inner and outer radius, thickness
post_r = 17.7;              // the posts on the back are this far from the center
post_d = 4.6;               // thick enough that the washer's screws don't split them
post_angles = [45, 135, 225, 315];
washer = [15.0, 22.2, 2.0]; // inner and outer radius, thickness
washer_arc = 130;           // each half spans this many degrees, so it fits through the thread

// The posts the collar turns on, with pilot holes for the washer's screws.
// They're part of the housing's back.
module collar_posts()
{
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
            // Turned over with a rotation, not a mirror, which would reverse
            // the thread
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

// The parts laid out to print: the collar grip down, the washer halves and
// the gasket flat
module collar_print() rotate([180, 0, 0]) translate([0, 0, -z_grip - collar_t]) collar();
module washer_print() for (i = [0, 1]) translate([0, i * 18, 0]) rotate([0, 0, 90]) washer_half();
