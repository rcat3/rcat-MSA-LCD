/*
 * The images shown on screen, in swipe order.
 *
 * To add an image, put it in imgs/ (PNG, BMP, JPG or animated GIF), then
 * declare it and add it to the list below. The build converts everything in
 * imgs/ to the right size for the board's screen (see tools/img2c.py), and
 * images that aren't listed here are left out of the firmware by the linker.
 *
 * IMAGE(name, is_gif): name is the image's file name without the extension.
 * It's also what the Startup Image setting shows and saves.
 */
#include <string.h>

#include "media/image_list.h"

#define IMAGE(name, is_gif) { #name, &name, is_gif }

LV_IMAGE_DECLARE(home);
LV_IMAGE_DECLARE(RCatLogo);
LV_IMAGE_DECLARE(nonbinary);
LV_IMAGE_DECLARE(hal9000);
LV_IMAGE_DECLARE(evileye);

const media_image_t media_images[] = {
    IMAGE(home,      false),
    IMAGE(RCatLogo,  false),
    IMAGE(nonbinary, false),
    IMAGE(hal9000,   false),
    IMAGE(evileye,   true),
};

const size_t media_image_count = sizeof(media_images) / sizeof(media_images[0]);

int media_find_image(const char *name)
{
    for (size_t i = 0; i < media_image_count; i++)
    {
        if (strcmp(media_images[i].name, name) == 0)
        {
            return (int)i;
        }
    }
    return -1;
}
