#ifndef RCAT_MEDIA_LIBRARY_H
#define RCAT_MEDIA_LIBRARY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MEDIA_NAME_LEN 24

typedef struct
{
    char name[MEDIA_NAME_LEN];  // file name without the extension
    const void *src;            // LVGL image source (lv_image_dsc_t or lv_draw_buf_t)
    bool is_gif;
    uint16_t scale;             // for GIFs: scale to show at, LV_SCALE_NONE (256) = as is
} media_item_t;

/* Start with the images built into the firmware. Call after lv_init(). */
void media_init(void);

/* Load the images in a folder on storage (its "images" subfolder if there is
 * one) and put them before the built-in images, sorted by name. Static
 * images are decoded and scaled for a screen_size x screen_size screen. GIFs
 * are loaded as they are, with a scale to show them at. Returns how many
 * were loaded. */
size_t media_load_folder(const char *path, int32_t screen_size);

size_t media_count(void);
const media_item_t *media_get(size_t index);

/* Index of the image with the given name, or -1 if there isn't one. */
int media_find(const char *name);

#endif
