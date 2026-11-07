// The 1.28" housing, assembled, for the documentation's pictures
include <../../hardware/boards/waveshare_touch_lcd_1_28/msa_housing.scad>
color("#3c3c3c") body();
color("#4a4a4a") bezel();
color("#101218") translate([0, 0, z_glass - glass_t]) cylinder(r = glass_r, h = glass_t, $fn = 128);
