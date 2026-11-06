// Housing that screws the Waveshare ESP32-S3-Touch-LCD-1.46 (the version
// with the cover glass) into the voice port of an MSA Millennium mask.
//
// The board sits in a cup outside the mask with its glass flush with the
// front. The cup's back plate carries the MSA thread, and the space inside
// the thread holds the battery and power switch, like the 1.28" design this
// is based on (see README.md for credits and license).
//
// The board's dimensions are in board.scad, and the thread is in
// hardware/masks/msa_millennium.
//
// Render the part to print with:  openscad -o housing.stl msa_housing.scad
// Set `part` to "lens_fit" or "thread_test" for the quick test prints.

part = "housing";           // "housing", "fit_test", "lens_fit" or "thread_test"

include <board.scad>

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

// Flip so the front is on the print bed
if (part == "housing") rotate([180, 0, 0]) housing();
else if (part == "fit_test") rotate([180, 0, 0]) housing(with_thread = false);
else if (part == "lens_fit") rotate([180, 0, 0]) translate([0, 0, -cup_top]) lens_fit();
else if (part == "thread_test") thread_test();
