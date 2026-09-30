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
#include "audio/audio_level.h"
#include "hal.h"
#include "lvgl.h"
#include "media/media_library.h"
#include "ui/kitt.h"
#include "ui/ui.h"
#include "ui/ui_internal.h"
#include "ui/voice_ring.h"

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
static int kitt_tile = -1;      // -1 when the board has no microphone
static uint8_t blank_tile;
static int ring_tile = -1;      // -1 when the board has no microphone
static lv_obj_t *ring_switch;
static lv_obj_t *ring_roller;
static int32_t screen_size;
static lv_timer_t *rotation_timer;
static uint8_t pending_rotation;

/* Scale a size or offset from the 240x240 design to this screen. */
static int32_t px(int32_t design_px)
{
    return design_px * screen_size / DESIGN_SIZE;
}

int32_t ui_px(int32_t design_px)
{
    return px(design_px);
}

static void setup_theme(void);
static void build_tiles(void);
static void add_pic_tile(lv_obj_t *tv, const media_item_t *pic, uint8_t num);
static void add_kitt_tile(lv_obj_t *tv, uint8_t num);
static void add_black_tile(lv_obj_t *tv, uint8_t num);
static void add_voice_ring_tile(lv_obj_t *tv, uint8_t num);
static void update_voice_ring(void);
static void tileview_event_cb(lv_event_t *e);
static void voice_ring_event_cb(lv_event_t *e);
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
    if (audio_level_available())
    {
        voice_ring_create();
    }
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

    // Images, the KITT voice box (if there's a microphone), the black tile,
    // then the settings: voice ring (if there's a microphone), brightness,
    // rotation and startup image.
    bool has_audio = audio_level_available();
    tile_count = media_count() + (has_audio ? 2 : 0) + 4;

    uint8_t num = 0;
    for (size_t i = 0; i < media_count(); i++)
    {
        add_pic_tile(tileview, media_get(i), num++);
    }
    if (has_audio)
    {
        kitt_tile = num;
        add_kitt_tile(tileview, num++);
    }
    blank_tile = num;
    add_black_tile(tileview, num++);
    if (has_audio)
    {
        ring_tile = num;
        add_voice_ring_tile(tileview, num++);
    }
    add_brightness_tile(tileview, num++);
    add_rotation_tile(tileview, num++);
    add_startup_tile(tileview, num++);

    lv_tileview_set_tile_by_index(tileview, 0, startup_tile_index(), LV_ANIM_OFF);
    lv_obj_add_event_cb(tileview, tileview_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    update_voice_ring();
}

/********************************************************************************
function:	Show the voice ring if it's switched on and the current tile is one
            it belongs on: the images, the black tile and its own settings tile
parameter:
********************************************************************************/
static void update_voice_ring(void)
{
    if (ring_tile < 0)
    {
        return;
    }

    int tile = lv_obj_get_index(lv_tileview_get_tile_active(tileview));
    bool on_tile = tile < (int)media_count() || tile == blank_tile || tile == ring_tile;

    voice_ring_set_color(settings_get()->voice_ring_color);
    voice_ring_set_visible(settings_get()->voice_ring && on_tile);
}

static void tileview_event_cb(lv_event_t *e)
{
    update_voice_ring();
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
        return blank_tile;
    }
    if (strcmp(name, SETTINGS_STARTUP_KITT) == 0 && kitt_tile >= 0)
    {
        return kitt_tile;
    }

    // Unknown names (e.g. an image that's since been removed, or KITT on a
    // board without a microphone) fall back to the first image.
    int index = media_find(name);
    return (index < 0) ? 0 : index;
}

static void add_pic_tile(lv_obj_t *tv, const media_item_t *pic, uint8_t num)
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
        lv_gif_set_src(this_img, pic->src);
        // Always loop forever. Some GIFs store a loop count, and LVGL (9.5, 9.6) reads
        // counts above 32767 as negative, which makes it stop after one pass.
        lv_gif_set_loop_count(this_img, 0);
        // GIFs from the SD card may need scaling for this screen.
        if (pic->scale != LV_SCALE_NONE)
        {
            lv_image_set_scale(this_img, pic->scale);
        }
    }
    else
    {
        this_img = lv_image_create(this_tile);
        lv_image_set_src(this_img, pic->src);
    }

    lv_obj_align(this_img, LV_ALIGN_CENTER, 0, 0);
}

static void add_kitt_tile(lv_obj_t *tv, uint8_t num)
{
    lv_obj_t *this_tile = lv_tileview_add_tile(tv, 0, num, tile_direction(num));
    lv_obj_set_style_bg_color(this_tile, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(this_tile, LV_OPA_COVER, 0);
    kitt_create(this_tile);
}

static void add_black_tile(lv_obj_t *tv, uint8_t num)
{
    lv_obj_t *this_tile = lv_tileview_add_tile(tv, 0, num, tile_direction(num));
    lv_obj_set_style_bg_color(this_tile, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(this_tile, LV_OPA_COVER, 0);
}

static void add_voice_ring_tile(lv_obj_t *tv, uint8_t num)
{
    lv_obj_t *ring_tile_obj = lv_tileview_add_tile(tv, 0, num, tile_direction(num));

    lv_obj_t *title = lv_label_create(ring_tile_obj);
    lv_label_set_text(title, "Voice Ring");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, px(32));

    ring_switch = lv_switch_create(ring_tile_obj);
    if (settings_get()->voice_ring)
    {
        lv_obj_add_state(ring_switch, LV_STATE_CHECKED);
    }
    lv_obj_align_to(ring_switch, title, LV_ALIGN_OUT_BOTTOM_MID, 0, px(8));
    lv_obj_add_event_cb(ring_switch, voice_ring_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    uint8_t color = settings_get()->voice_ring_color;
    if (color >= voice_ring_color_count())
    {
        color = 0;
    }
    ring_roller = lv_roller_create(ring_tile_obj);
    lv_roller_set_options(ring_roller, voice_ring_color_options(), LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(ring_roller, 3);
    lv_roller_set_selected(ring_roller, color, LV_ANIM_OFF);
    lv_obj_align(ring_roller, LV_ALIGN_CENTER, 0, px(28));
    lv_obj_add_event_cb(ring_roller, voice_ring_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

static void voice_ring_event_cb(lv_event_t *e)
{
    settings_set_voice_ring(lv_obj_has_state(ring_switch, LV_STATE_CHECKED),
                            lv_roller_get_selected(ring_roller));
    update_voice_ring();
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

    // Roller options, in the same order as the tiles: every image name, then
    // "KITT" if there's a microphone, then "Blank", separated by newlines.
    size_t len = sizeof("KITT\nBlank");
    for (size_t i = 0; i < media_count(); i++)
    {
        len += strlen(media_get(i)->name) + 1;
    }
    char *options = lv_malloc(len);
    options[0] = '\0';
    for (size_t i = 0; i < media_count(); i++)
    {
        strcat(options, media_get(i)->name);
        strcat(options, "\n");
    }
    if (kitt_tile >= 0)
    {
        strcat(options, "KITT\n");
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

    // The roller options are in tile order.
    if (selected < media_count())
    {
        settings_set_startup_image(media_get(selected)->name);
    }
    else if ((int)selected == kitt_tile)
    {
        settings_set_startup_image(SETTINGS_STARTUP_KITT);
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
