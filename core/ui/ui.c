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

#include "hal.h"
#include "lvgl.h"
#include "media/image_list.h"
#include "ui/ui.h"

#define BATTERY_UPDATE_MS 1000

static lv_obj_t *tileview;
static lv_obj_t *battery_label;
static lv_obj_t *brightness_value_label;

static uint8_t brightness = UI_INITIAL_BRIGHTNESS;
static uint16_t img_rotation = 0;   // tenths of a degree

static void build_tiles(void);
static void apply_rotation(void);
static void add_pic_tile(lv_obj_t *tv, const media_image_t *pic, uint8_t num);
static void add_black_tile(lv_obj_t *tv, uint8_t num);
static void add_brightness_tile(lv_obj_t *tv, uint8_t num);
static void add_rotation_tile(lv_obj_t *tv, uint8_t num);
static void brightness_slider_event_cb(lv_event_t *e);
static void rotation_roller_event_cb(lv_event_t *e);
static void battery_timer_cb(lv_timer_t *t);

/********************************************************************************
function:	Build the UI and start the battery voltage updates
parameter:
********************************************************************************/
void ui_init(void)
{
    build_tiles();

    float volts;
    if (hal_battery_voltage(&volts))
    {
        lv_timer_t *timer = lv_timer_create(battery_timer_cb, BATTERY_UPDATE_MS, NULL);
        lv_timer_ready(timer);
    }
}

/********************************************************************************
function:	Create the tile view
parameter:
********************************************************************************/
static void build_tiles(void)
{
    tileview = lv_tileview_create(lv_scr_act());
    lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);

    uint8_t num = 0;
    for (size_t i = 0; i < media_image_count; i++)
    {
        add_pic_tile(tileview, &media_images[i], num++);
    }
    add_black_tile(tileview, num++);
    add_brightness_tile(tileview, num++);
    add_rotation_tile(tileview, num++);
}

static lv_dir_t tile_direction(uint8_t num)
{
    // The first tile can only swipe down. All others can swipe up and down.
    return (num == 0) ? LV_DIR_BOTTOM : (LV_DIR_TOP | LV_DIR_BOTTOM);
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
        lv_gif_set_src(this_img, pic->img);
    }
    else
    {
        this_img = lv_img_create(this_tile);
        lv_img_set_src(this_img, pic->img);
        lv_img_set_angle(this_img, img_rotation);
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
    lv_obj_set_size(brightness_slider, 200, 20);
    lv_slider_set_range(brightness_slider, 1, 100);
    lv_slider_set_value(brightness_slider, brightness, LV_ANIM_OFF);
    lv_obj_center(brightness_slider);
    lv_obj_add_event_cb(brightness_slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    brightness_value_label = lv_label_create(brightness_tile);
    lv_label_set_text_fmt(brightness_value_label, "%d%%", brightness);
    lv_obj_align_to(brightness_value_label, brightness_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    lv_obj_t *brightness_label = lv_label_create(brightness_tile);
    lv_label_set_text(brightness_label, "Brightness");
    lv_obj_align_to(brightness_label, brightness_slider, LV_ALIGN_OUT_TOP_MID, 0, -10);

    // Battery voltage label
    battery_label = lv_label_create(brightness_tile);
    lv_label_set_text(battery_label, "");
    lv_obj_align(battery_label, LV_ALIGN_TOP_MID, 0, 35);
}

static void add_rotation_tile(lv_obj_t *tv, uint8_t num)
{
    // Last tile, so it can only swipe up.
    lv_obj_t *rotation_tile = lv_tileview_add_tile(tv, 0, num, LV_DIR_TOP);

    // Image rotation roller.
    lv_obj_t *rotation_roller = lv_roller_create(rotation_tile);
    lv_roller_set_options(rotation_roller,
                          "0°\n90°\n180°\n270°", LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(rotation_roller, 3);
    lv_roller_set_selected(rotation_roller, img_rotation / 900, LV_ANIM_OFF);
    lv_obj_center(rotation_roller);
    lv_obj_add_event_cb(rotation_roller, rotation_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *rotation_label = lv_label_create(rotation_tile);
    lv_label_set_text(rotation_label, "Img Rotation");
    lv_obj_align_to(rotation_label, rotation_roller, LV_ALIGN_OUT_TOP_MID, 0, -10);
}

static void brightness_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *brightness_slider = lv_event_get_target(e);
    brightness = lv_slider_get_value(brightness_slider);

    lv_label_set_text_fmt(brightness_value_label, "%d%%", brightness);
    lv_obj_align_to(brightness_value_label, brightness_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    hal_backlight_set(brightness);
}

static void rotation_roller_event_cb(lv_event_t *e)
{
    lv_obj_t *rotation_roller = lv_event_get_target(e);

    // Roller options are 0, 90, 180 and 270 degrees.
    img_rotation = lv_roller_get_selected(rotation_roller) * 900;

    apply_rotation();
}

/********************************************************************************
function:	Rotate the static images in place and jump back to the first image
parameter:
********************************************************************************/
static void apply_rotation(void)
{
    uint32_t tile_count = lv_obj_get_child_cnt(tileview);
    for (uint32_t i = 0; i < tile_count; i++)
    {
        lv_obj_t *img = lv_obj_get_child(lv_obj_get_child(tileview, i), 0);

        // Exact class check, so animated GIFs (a subclass of lv_img) are skipped.
        if (lv_obj_check_type(img, &lv_img_class))
        {
            lv_img_set_angle(img, img_rotation);
        }
    }

    lv_obj_set_tile_id(tileview, 0, 0, LV_ANIM_OFF);
}

static void battery_timer_cb(lv_timer_t *t)
{
    float volts;
    char label_text[20];

    if (hal_battery_voltage(&volts))
    {
        snprintf(label_text, sizeof(label_text), "Battery: %.2fV", volts);
        lv_label_set_text(battery_label, label_text);
    }
}
