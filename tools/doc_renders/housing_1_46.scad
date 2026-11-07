// The 1.46" simple housing, assembled, for the documentation's pictures
include <../../hardware/boards/waveshare_esp32s3_touch_lcd_1_46/msa_housing.scad>
color("#3c3c3c") housing();
color("#101218") translate([0, 0, -2]) cylinder(d = lens_d, h = 2);
