#ifndef RCAT_IMAGE_LIST_H
#define RCAT_IMAGE_LIST_H

#include <stdbool.h>
#include <stddef.h>
#include "lvgl.h"

typedef struct
{
    const char *name;
    const lv_image_dsc_t *img;
    bool is_gif;
} media_image_t;

/* Images compiled into the firmware, in the order they appear on screen. */
extern const media_image_t media_images[];
extern const size_t media_image_count;

/* Index of the image with the given name, or -1 if there isn't one. */
int media_find_image(const char *name);

#endif
