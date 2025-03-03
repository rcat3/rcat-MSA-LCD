Rubbercat's MSA LCD Software


This code is hacked together from the Waveshare LVGL example program.  



To build:

cd build
cmake ..
make

A .uf2 file will be generated.  Hold down the BOOT button on the target and press RESET.  Release the BOOT button.

The target device will appear as a USB mass storage device.  Copy the .uf2 file to this device.  The target will
reset and start running when complete.
