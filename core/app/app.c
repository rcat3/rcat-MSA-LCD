#include "app.h"
#include "app/settings.h"
#include "audio/audio_level.h"
#include "hal.h"
#include "media/media_library.h"
#include "lvgl.h"
#include "ui/ui.h"

/* Built-in images, plus any on the SD card (shown first). */
static void load_images(void)
{
    media_init();

    const char *storage_path;
    if (!rcat_hal_storage_mount(&storage_path))
    {
        return;
    }

    // Reading the card can take a moment, so say what's happening.
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_t *label = lv_label_create(screen);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "Loading images...");
    lv_obj_center(label);
    lv_refr_now(NULL);

    lv_display_t *disp = lv_display_get_default();
    int32_t screen_size = LV_MIN(lv_display_get_horizontal_resolution(disp),
                                 lv_display_get_vertical_resolution(disp));
    media_load_folder(storage_path, screen_size);

    lv_obj_delete(label);
}

void app_run(void)
{
    if (rcat_hal_init() != 0)
    {
        return;
    }
    settings_init();
    rcat_hal_backlight_set(settings_get()->brightness);

    lv_init();
    rcat_hal_lvgl_register();
    rcat_hal_display_set_rotation(settings_get()->rotation);
    load_images();
    audio_level_start();    // does nothing on boards without a microphone
    ui_init();

    while (1)
    {
        lv_timer_handler();
        rcat_hal_delay_ms(5);
    }
}
