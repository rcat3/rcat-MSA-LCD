/*
 * Images feature: a page per image (from the SD card, then built in), and a
 * blank black page at the end.
 */
#include "app/settings.h"
#include "media/media_library.h"
#include "ui/ui_internal.h"

static bool build(ui_feature_t *feature);

static ui_feature_t *images_feature;

const ui_feature_def_t feature_images = {
    .name = "Images",
    .build = build,
};

static void add_image_page(ui_feature_t *feature, const media_item_t *pic)
{
    lv_obj_t *page = ui_feature_add_page(feature, true);
    lv_obj_t *img;

    if (pic->is_gif)
    {
        img = lv_gif_create(page);
        // Decode straight to RGB565 to use half the RAM of the ARGB8888 default.
        lv_gif_set_color_format(img, LV_COLOR_FORMAT_RGB565);
        lv_gif_set_src(img, pic->src);
        // Always loop forever. Some GIFs store a loop count, and LVGL (9.5, 9.6)
        // reads counts above 32767 as negative, which makes it stop after one pass.
        lv_gif_set_loop_count(img, 0);
        // GIFs from the SD card may need scaling for this screen.
        if (pic->scale != LV_SCALE_NONE)
        {
            lv_image_set_scale(img, pic->scale);
        }
    }
    else
    {
        img = lv_image_create(page);
        lv_image_set_src(img, pic->src);
    }
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

    ui_add_startup_choice(feature, page, pic->name, pic->name);
}

bool feature_images_on_screen(void)
{
    return images_feature != NULL && ui_feature_is_current(images_feature);
}

static bool build(ui_feature_t *feature)
{
    images_feature = feature;

    for (size_t i = 0; i < media_count(); i++)
    {
        add_image_page(feature, media_get(i));
    }

    lv_obj_t *blank = ui_feature_add_page(feature, true);
    ui_add_startup_choice(feature, blank, "Blank", SETTINGS_STARTUP_BLANK);
    return true;
}
