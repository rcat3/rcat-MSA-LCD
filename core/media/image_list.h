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
} builtin_image_t;

/* Images compiled into the firmware, in the order they appear on screen. */
extern const builtin_image_t builtin_images[];
extern const size_t builtin_image_count;

#endif
