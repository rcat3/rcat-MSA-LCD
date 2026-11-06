// What sticks out of the front of the mask near the voice port, simplified
// from a 3D scan of a mask: the filter mount below the port and the hose that
// loops from it to the right. Housings show it in their previews to check
// nothing hangs into it. Include this file.
//
// Coordinates are the voice port's: the center of the front of its threaded
// rim is the origin, z points out of the mask, and y points up, towards the
// eyes, with the eyes level. The mask in the scan sat a few degrees crooked
// on the head, so allow a few degrees either way. The shapes are the highest
// points of the scan across their width, so they're a little bigger than the
// real thing, never smaller.

port_rim_r = [26, 31.5];    // the threaded ring the housing screws into, inside and outside
port_rim_h = 10;            // how far it stands out of the mask around it

// The filter mount seen from the side: [down from the port's center, out
// from the rim], every 3 mm
mount_profile = [[-38, -11.8], [-41, 3.8], [-44, 4.5], [-47, 5.0], [-50, 7.8], [-53, 10.5],
                 [-56, 13.4], [-59, 13.3], [-62, 11.0], [-65, 8.4], [-68, 6.1], [-71, 7.3],
                 [-74, 9.8], [-77, 12.3], [-80, 11.7], [-83, 8.4], [-86, 5.4], [-89, 2.5],
                 [-92, -0.5], [-95, -3.7], [-98, -7.4], [-101, -10.6], [-104, -16.1]];
mount_x = [-27, 24];        // its width
// The hose, as points along its middle: [x, y, how far out it reaches]
hose_path = [[25, -82, 11.7], [33, -68, 11.0], [40, -64, 12.3], [46, -60, 12.5],
             [51, -54, 11.9], [55, -50, 8.4]];
hose_r = 6;
keepout_floor = -16;        // the shapes stop here, behind the rim

module port_rim()
{
    translate([0, 0, -port_rim_h])
        difference()
        {
            cylinder(r = port_rim_r[1], h = port_rim_h, $fn = 96);
            translate([0, 0, -1]) cylinder(r = port_rim_r[0], h = port_rim_h + 2, $fn = 96);
        }
}

module filter_mount()
{
    pts = concat([[mount_profile[0][0], keepout_floor]], mount_profile,
                 [[mount_profile[len(mount_profile) - 1][0], keepout_floor]]);
    translate([mount_x[0], 0, 0]) rotate([90, 0, 90])
        linear_extrude(mount_x[1] - mount_x[0]) polygon(pts);
}

module hose()
{
    for (i = [0 : len(hose_path) - 2])
        hull()
            for (p = [hose_path[i], hose_path[i + 1]])
                translate([p[0], p[1], keepout_floor]) cylinder(r = hose_r, h = p[2] - keepout_floor, $fn = 24);
}

// Everything, in the port's coordinates
module mask_keepout()
{
    port_rim();
    filter_mount();
    hose();
}
