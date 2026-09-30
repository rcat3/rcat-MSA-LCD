/*
 * The images built into the firmware, in swipe order. Images from the SD
 * card (on boards that have one) are shown before these.
 *
 * To add an image, put it in imgs/ (PNG, BMP, JPG or animated GIF), then
 * declare it and add it to the list below. The build converts everything in
 * imgs/ to the right size for the board's screen (see tools/img2c.py), and
 * images that aren't listed here are left out of the firmware by the linker.
 *
 * IMAGE(name, is_gif): name is the image's file name without the extension.
 * It's also what the Startup Image setting shows and saves.
 */
#include "media/image_list.h"

#define IMAGE(name, is_gif) { #name, &name, is_gif }

LV_IMAGE_DECLARE(home);
LV_IMAGE_DECLARE(RCatLogo);
LV_IMAGE_DECLARE(nonbinary);
LV_IMAGE_DECLARE(hal9000);
LV_IMAGE_DECLARE(evileye);

const builtin_image_t builtin_images[] = {
    IMAGE(home,      false),
    IMAGE(RCatLogo,  false),
    IMAGE(nonbinary, false),
    IMAGE(hal9000,   false),
    IMAGE(evileye,   true),
};

const size_t builtin_image_count = sizeof(builtin_images) / sizeof(builtin_images[0]);
