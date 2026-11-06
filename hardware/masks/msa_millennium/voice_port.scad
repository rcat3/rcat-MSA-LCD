// The MSA Millennium's voice port thread, shared by every housing that
// screws into the mask. Include this file.
//
// vendor/vpu_thread.stl is the thread from the 1.28" design (see README.md
// for credits), cleaned up into a single solid with the original's inner lip
// removed so the inside is a plain 42 mm bore. Its front face is at z = 0 and
// it screws into the mask from the z = 8 end.

use <vpu_thread_mesh.scad>

thread_scale = 0.997;       // scales the thread's diameter. From the test rings: 99.7% fits best
                            // (98% is too loose, 102% doesn't fit)
thread_lead_in = [1.5, 1.2];  // bevel on the start of the thread (length, depth) to help it catch
thread_length = 8;          // how far the thread reaches into the port

// The thread at scale s, with the start of it beveled so it catches more
// easily
module vpu_thread(s = thread_scale)
{
    thread_r = 26 * s;
    difference()
    {
        scale([s, s, 1]) vpu_thread_mesh();
        translate([0, 0, thread_length - thread_lead_in[0]])
            difference()
            {
                cylinder(r = thread_r + 2, h = thread_lead_in[0] + 1);
                translate([0, 0, -0.01])
                    cylinder(r1 = thread_r + 0.01, r2 = thread_r - thread_lead_in[1], h = thread_lead_in[0] + 0.02);
            }
    }
}

// Thin thread rings at a few scales, to find the best fit in the mask. Each
// has a bar across it to grip, so it can be screwed all the way in and out.
module thread_test(scales = [0.994, 0.997, 1.000])
{
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
