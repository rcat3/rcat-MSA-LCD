// The 1.75" speaker housing, assembled, for the documentation's pictures
include <../../hardware/boards/waveshare_esp32s3_touch_amoled_1_75/msa_speaker_housing.scad>
color("#3c3c3c") body();
color("#3c3c3c") chin();
color("#4a4a4a") back();
color("#4a4a4a") collar();
color("#4a4a4a") usb_plug();
color("#101218") translate([0, 0, -lens_t]) cylinder(d = lens_d, h = lens_t);
