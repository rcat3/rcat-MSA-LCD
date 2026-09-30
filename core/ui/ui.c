/*****************************************************************************
* | File      	:   ui.c
* | Author      :   Waveshare team, Rubber Cat
* | Function    :   Tile view with images and settings screens
* | Info        :   Originally based on the Waveshare LVGL demo
*----------------
* |	This version:   V1.0
* | Date        :   2023-12-23
* | Info        :
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/

#include <stdio.h>
#include <string.h>

#include "app/settings.h"
#include "hal.h"
#include "lvgl.h"
#include "media/image_list.h"
#include "ui/ui.h"

#define BATTERY_UPDATE_MS 1000

// Apply a new rotation only once the roller has been left alone this long, so
// flicking past options doesn't rotate the screen under your finger.
#define ROTATION_APPLY_DELAY_MS 1000

// The layout was designed for a 240x240 screen and is scaled up for bigger ones.
#define DESIGN_SIZE 240

static lv_obj_t *tileview;
static lv_obj_t *battery_label;
static lv_obj_t *brightness_value_label;
static uint8_t tile_count;
static int32_t screen_size;
static lv_timer_t *rotation_timer;
static uint8_t pending_rotation;

/* Scale a size or offset from the 240x240 design to this screen. */
static int32_t px(int32_t design_px)
{
    return design_px * screen_size / DESIGN_SIZE;
}

static void setup_theme(void);
static void build_tiles(void);
static void add_pic_tile(lv_obj_t *tv, const media_image_t *pic, uint8_t num);
static void add_black_tile(lv_obj_t *tv, uint8_t num);
static void add_brightness_tile(lv_obj_t *tv, uint8_t num);
static void add_rotation_tile(lv_obj_t *tv, uint8_t num);
static void add_startup_tile(lv_obj_t *tv, uint8_t num);
static uint8_t startup_tile_index(void);
static void brightness_slider_event_cb(lv_event_t *e);
static void rotation_roller_event_cb(lv_event_t *e);
static void startup_roller_event_cb(lv_event_t *e);
static void rotation_apply_timer_cb(lv_timer_t *t);
static void battery_timer_cb(lv_timer_t *t);

/********************************************************************************
function:	Build the UI and start the battery voltage updates
parameter:
********************************************************************************/
void ui_init(void)
{
    setup_theme();
    build_tiles();

    float volts;
    if (rcat_hal_battery_voltage(&volts))
    {
        lv_timer_t *timer = lv_timer_create(battery_timer_cb, BATTERY_UPDATE_MS, NULL);
        lv_timer_ready(timer);
    }
}

/********************************************************************************
function:	Scale the theme (font, padding) to the screen size
parameter:
********************************************************************************/
static void setup_theme(void)
{
    lv_display_t *disp = lv_display_get_default();
    screen_size = LV_MIN(lv_display_get_horizontal_resolution(disp),
                         lv_display_get_vertical_resolution(disp));

    if (screen_size == DESIGN_SIZE)
    {
        return;     // the defaults are what the layout was designed with
    }

    // The theme sizes its padding and knobs from the DPI.
    lv_display_set_dpi(disp, px(LV_DPI_DEF));

    const lv_font_t *font = LV_FONT_DEFAULT;
#if LV_FONT_MONTSERRAT_24
    if (screen_size >= 360)
    {
        font = &lv_font_montserrat_24;
    }
#endif
    lv_theme_t *theme = lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_BLUE),
                                              lv_palette_main(LV_PALETTE_RED),
                                              false, font);
    lv_display_set_theme(disp, theme);
}

/********************************************************************************
function:	Create the tile view
parameter:
********************************************************************************/
static void build_tiles(void)
{
    tileview = lv_tileview_create(lv_screen_active());
    lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);

    // Images, the black tile, then brightness, rotation and startup image.
    tile_count = media_image_count + 4;

    uint8_t num = 0;
    for (size_t i = 0; i < media_image_count; i++)
    {
        add_pic_tile(tileview, &media_images[i], num++);
    }
    add_black_tile(tileview, num++);
    add_brightness_tile(tileview, num++);
    add_rotation_tile(tileview, num++);
    add_startup_tile(tileview, num++);

    lv_tileview_set_tile_by_index(tileview, 0, startup_tile_index(), LV_ANIM_OFF);
}

static lv_dir_t tile_direction(uint8_t num)
{
    // The first tile can only swipe down and the last can only swipe up.
    if (num == 0)
    {
        return LV_DIR_BOTTOM;
    }
    if (num == tile_count - 1)
    {
        return LV_DIR_TOP;
    }
    return LV_DIR_TOP | LV_DIR_BOTTOM;
}

/********************************************************************************
function:	Tile to show at startup, from the saved startup image name
parameter:
********************************************************************************/
static uint8_t startup_tile_index(void)
{
    const char *name = settings_get()->startup_image;

    if (strcmp(name, SETTINGS_STARTUP_BLANK) == 0)
    {
        // The black tile comes right after the images.
        return media_image_count;
    }

    // Unknown names (e.g. an image that's since been removed) fall back to the first image.
    int index = media_find_image(name);
    return (index < 0) ? 0 : index;
}

static void add_pic_tile(lv_obj_t *tv, const media_image_t *pic, uint8_t num)
{
    lv_obj_t *this_img;

    // Add a tile...
    lv_obj_t *this_tile = lv_tileview_add_tile(tv, 0, num, tile_direction(num));

    lv_obj_set_style_bg_color(this_tile, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(this_tile, LV_OPA_COVER, 0);

    // Add an image to the tile.
    if (pic->is_gif)
    {
        this_img = lv_gif_create(this_tile);
        // Decode straight to RGB565 to use half the RAM of the ARGB8888 default.
        lv_gif_set_color_format(this_img, LV_COLOR_FORMAT_RGB565);
        lv_gif_set_src(this_img, pic->img);
        // Always loop forever. Some GIFs store a loop count, and LVGL 9.5 reads
        // counts above 32767 as negative, which makes it stop after one pass.
        lv_gif_set_loop_count(this_img, 0);
    }
    else
    {
        this_img = lv_image_create(this_tile);
        lv_image_set_src(this_img, pic->img);
    }

    lv_obj_align(this_img, LV_ALIGN_CENTER, 0, 0);
}

static void add_black_tile(lv_obj_t *tv, uint8_t num)
{
    lv_obj_t *this_tile = lv_tileview_add_tile(tv, 0, num, tile_direction(num));
    lv_obj_set_style_bg_color(this_tile, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(this_tile, LV_OPA_COVER, 0);
}

static void add_brightness_tile(lv_obj_t *tv, uint8_t num)
{
    lv_obj_t *brightness_tile = lv_tileview_add_tile(tv, 0, num, tile_direction(num));

    // Brightness slider
    lv_obj_t *brightness_slider = lv_slider_create(brightness_tile);
    lv_obj_set_size(brightness_slider, px(200), px(20));
    lv_slider_set_range(brightness_slider, 1, 100);
    lv_slider_set_value(brightness_slider, settings_get()->brightness, LV_ANIM_OFF);
    lv_obj_center(brightness_slider);
    lv_obj_add_event_cb(brightness_slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    brightness_value_label = lv_label_create(brightness_tile);
    lv_label_set_text_fmt(brightness_value_label, "%d%%", settings_get()->brightness);
    lv_obj_align_to(brightness_value_label, brightness_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, px(10));

    lv_obj_t *brightness_label = lv_label_create(brightness_tile);
    lv_label_set_text(brightness_label, "Brightness");
    lv_obj_align_to(brightness_label, brightness_slider, LV_ALIGN_OUT_TOP_MID, 0, px(-10));

    // Battery voltage label
    battery_label = lv_label_create(brightness_tile);
    lv_label_set_text(battery_label, "");
    lv_obj_align(battery_label, LV_ALIGN_TOP_MID, 0, px(35));
}

static void add_rotation_tile(lv_obj_t *tv, uint8_t num)
{
    lv_obj_t *rotation_tile = lv_tileview_add_tile(tv, 0, num, tile_direction(num));

    // Display rotation roller.
    lv_obj_t *rotation_roller = lv_roller_create(rotation_tile);
    lv_roller_set_options(rotation_roller,
                          "0°\n90°\n180°\n270°", LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(rotation_roller, 3);
    lv_roller_set_selected(rotation_roller, settings_get()->rotation, LV_ANIM_OFF);
    lv_obj_center(rotation_roller);
    lv_obj_add_event_cb(rotation_roller, rotation_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *rotation_label = lv_label_create(rotation_tile);
    lv_label_set_text(rotation_label, "Rotation");
    lv_obj_align_to(rotation_label, rotation_roller, LV_ALIGN_OUT_TOP_MID, 0, px(-10));
}

static void add_startup_tile(lv_obj_t *tv, uint8_t num)
{
    lv_obj_t *startup_tile = lv_tileview_add_tile(tv, 0, num, tile_direction(num));

    // Roller options: every image name, then "Blank", separated by newlines.
    size_t len = sizeof("Blank");
    for (size_t i = 0; i < media_image_count; i++)
    {
        len += strlen(media_images[i].name) + 1;
    }
    char *options = lv_malloc(len);
    options[0] = '\0';
    for (size_t i = 0; i < media_image_count; i++)
    {
        strcat(options, media_images[i].name);
        strcat(options, "\n");
    }
    strcat(options, "Blank");

    lv_obj_t *startup_roller = lv_roller_create(startup_tile);
    lv_roller_set_options(startup_roller, options, LV_ROLLER_MODE_NORMAL);
    lv_free(options);   // the roller keeps its own copy
    lv_roller_set_visible_row_count(startup_roller, 3);
    lv_roller_set_selected(startup_roller, startup_tile_index(), LV_ANIM_OFF);
    lv_obj_center(startup_roller);
    lv_obj_add_event_cb(startup_roller, startup_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *startup_label = lv_label_create(startup_tile);
    lv_label_set_text(startup_label, "Startup Image");
    lv_obj_align_to(startup_label, startup_roller, LV_ALIGN_OUT_TOP_MID, 0, px(-10));
}

static void brightness_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *brightness_slider = lv_event_get_target_obj(e);
    uint8_t brightness = lv_slider_get_value(brightness_slider);

    lv_label_set_text_fmt(brightness_value_label, "%d%%", brightness);
    lv_obj_align_to(brightness_value_label, brightness_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, px(10));
    rcat_hal_backlight_set(brightness);
    settings_set_brightness(brightness);
}

static void rotation_roller_event_cb(lv_event_t *e)
{
    lv_obj_t *rotation_roller = lv_event_get_target_obj(e);

    // Roller options are 0, 90, 180 and 270 degrees.
    pending_rotation = lv_roller_get_selected(rotation_roller);

    if (rotation_timer == NULL)
    {
        rotation_timer = lv_timer_create(rotation_apply_timer_cb, ROTATION_APPLY_DELAY_MS, NULL);
    }
    lv_timer_reset(rotation_timer);
    lv_timer_resume(rotation_timer);
}

static void rotation_apply_timer_cb(lv_timer_t *t)
{
    lv_timer_pause(t);

    if (pending_rotation != settings_get()->rotation)
    {
        rcat_hal_display_set_rotation(pending_rotation);
        settings_set_rotation(pending_rotation);
    }
}

static void startup_roller_event_cb(lv_event_t *e)
{
    lv_obj_t *startup_roller = lv_event_get_target_obj(e);
    uint16_t selected = lv_roller_get_selected(startup_roller);

    if (selected < media_image_count)
    {
        settings_set_startup_image(media_images[selected].name);
    }
    else
    {
        settings_set_startup_image(SETTINGS_STARTUP_BLANK);
    }
}

static void battery_timer_cb(lv_timer_t *t)
{
    float volts;
    char label_text[20];

    if (rcat_hal_battery_voltage(&volts))
    {
        snprintf(label_text, sizeof(label_text), "Battery: %.2fV", volts);
        lv_label_set_text(battery_label, label_text);
    }
}
