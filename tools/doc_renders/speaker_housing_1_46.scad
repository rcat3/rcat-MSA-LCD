// The 1.46" speaker housing, assembled, for the documentation's pictures
include <../../hardware/boards/waveshare_esp32s3_touch_lcd_1_46/msa_speaker_housing.scad>
color("#3c3c3c") body();
color("#4a4a4a") back();
color("#4a4a4a") collar();
color("#101218") translate([0, 0, -2]) cylinder(d = lens_d, h = 2);
