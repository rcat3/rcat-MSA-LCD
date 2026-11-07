#include "app.h"
#include "app/patterns.h"
#include "app/settings.h"
#include "audio/audio_level.h"
#include "audio/audio_player.h"
#include "audio/voice_lab.h"
#include "hal.h"
#include "media/media_library.h"
#include "lvgl.h"
#include "ui/ui.h"

// The logo is shown for at least this long at startup, while everything
// else (reading the SD card included) gets ready behind it.
#define SPLASH_MS           2000

// core/ui/assets/dronelab_logo.png, made by tools/gen_logo.py and converted
// for the screen by the build. It's drawn 480x480 and scaled to fit.
LV_IMAGE_DECLARE(dronelab_logo);
#define SPLASH_ART_SIZE     480

// The blinking cursor after its "> SYSTEM ONLINE" prompt: x, y, width,
// height in the 480x480 picture, as tools/gen_logo.py prints them
static const int32_t SPLASH_CURSOR[4] = { 322, 279, 8, 15 };
#define SPLASH_CURSOR_COLOR lv_color_hex(0x33ff33)   // the Terminal's phosphor green
#define SPLASH_BLINK_MS     500

typedef struct
{
    lv_obj_t *logo;
    lv_obj_t *cursor;
    lv_timer_t *blink;
    uint32_t shown_at;
} splash_t;

static void splash_blink_cb(lv_timer_t *timer)
{
    lv_obj_t *cursor = lv_timer_get_user_data(timer);
    lv_obj_set_hidden(cursor, !lv_obj_is_hidden(cursor));
}

/* Show the logo on a black screen, straight away, with its cursor. */
static void splash_show(splash_t *splash)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    splash->logo = lv_image_create(screen);
    lv_image_set_src(splash->logo, &dronelab_logo);
    lv_obj_center(splash->logo);

    // Placed where the prompt ends in the picture, scaled to the screen
    lv_display_t *disp = lv_display_get_default();
    int32_t w = lv_display_get_horizontal_resolution(disp);
    int32_t h = lv_display_get_vertical_resolution(disp);
    int32_t art = LV_MIN(w, h);
    splash->cursor = lv_obj_create(screen);
    lv_obj_remove_style_all(splash->cursor);
    lv_obj_set_style_bg_color(splash->cursor, SPLASH_CURSOR_COLOR, 0);
    lv_obj_set_style_bg_opa(splash->cursor, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_color(splash->cursor, SPLASH_CURSOR_COLOR, 0);
    lv_obj_set_style_shadow_width(splash->cursor, LV_MAX(2, art / 60), 0);
    lv_obj_set_style_shadow_opa(splash->cursor, LV_OPA_60, 0);
    lv_obj_set_pos(splash->cursor,
                   (w - art) / 2 + SPLASH_CURSOR[0] * art / SPLASH_ART_SIZE,
                   (h - art) / 2 + SPLASH_CURSOR[1] * art / SPLASH_ART_SIZE);
    lv_obj_set_size(splash->cursor, LV_MAX(1, SPLASH_CURSOR[2] * art / SPLASH_ART_SIZE),
                    LV_MAX(1, SPLASH_CURSOR[3] * art / SPLASH_ART_SIZE));
    splash->blink = lv_timer_create(splash_blink_cb, SPLASH_BLINK_MS, splash->cursor);

    lv_refr_now(NULL);
    splash->shown_at = lv_tick_get();
}

/* Keep the logo up, its cursor blinking, until SPLASH_MS after it appeared,
 * then take it down. */
static void splash_hide(splash_t *splash)
{
    while (lv_tick_elaps(splash->shown_at) < SPLASH_MS)
    {
        lv_timer_handler();
        rcat_hal_delay_ms(5);
    }
    lv_timer_delete(splash->blink);
    lv_obj_delete(splash->cursor);
    lv_obj_delete(splash->logo);
}

/* Built-in images, plus any on the SD card (shown first). storage_path is
 * NULL without a card. Reading the card can take a moment, behind the logo. */
static void load_images(const char *storage_path)
{
    media_init();

    if (storage_path == NULL)
    {
        return;
    }

    lv_display_t *disp = lv_display_get_default();
    int32_t screen_size = LV_MIN(lv_display_get_horizontal_resolution(disp),
                                 lv_display_get_vertical_resolution(disp));
    media_load_folder(storage_path, screen_size);
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
    splash_t splash;
    splash_show(&splash);

    const char *storage_path;
    if (!rcat_hal_storage_mount(&storage_path))
    {
        storage_path = NULL;
    }
    load_images(storage_path);
    audio_level_start();                // does nothing on boards without a microphone
    audio_player_init(storage_path);    // or a speaker
    voice_lab_init(storage_path);
    patterns_init(storage_path);        // for the Lovense feature

    splash_hide(&splash);
    ui_init();

    while (1)
    {
        lv_timer_handler();
        rcat_hal_delay_ms(5);
    }
}
