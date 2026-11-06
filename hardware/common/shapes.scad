// Small shape helpers shared by the housings. Include this file; it has no
// settings of its own.

// Boxes given as their two corners, [[x0, y0], [x1, y1]]
function box_center(b) = [(b[0][0] + b[1][0]) / 2, (b[0][1] + b[1][1]) / 2];
function box_size(b) = [b[1][0] - b[0][0], b[1][1] - b[0][1]];

// A box from z0 to z1, grown by grow all round in x and y
module slab(b, z0, z1, grow = 0)
{
    translate([b[0][0] - grow, b[0][1] - grow, z0])
        cube([box_size(b)[0] + 2 * grow, box_size(b)[1] + 2 * grow, z1 - z0]);
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
