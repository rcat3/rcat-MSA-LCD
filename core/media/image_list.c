/*
 * The images shown on screen, in swipe order.
 *
 * To add an image, convert it to a C array (see the README), drop the .c file
 * into core/media/images/, then declare it and add it to the list below.
 * Images that aren't listed here are left out of the firmware by the linker.
 */
#include "media/image_list.h"

LV_IMG_DECLARE(home);
LV_IMG_DECLARE(RCatLogo);
LV_IMG_DECLARE(nonbinary);
LV_IMG_DECLARE(hal9000);
LV_IMG_DECLARE(evileye);

const media_image_t media_images[] = {
    { &home,      false },
    { &RCatLogo,  false },
    { &nonbinary, false },
    { &hal9000,   false },
    { &evileye,   true  },
};

const size_t media_image_count = sizeof(media_images) / sizeof(media_images[0]);
