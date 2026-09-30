/*****************************************************************************
* | File      	:   ui.c
* | Author      :   Waveshare team, Rubber Cat
* | Function    :   Feature and page navigation
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

#include <string.h>

#include "app/settings.h"
#include "lvgl.h"
#include "ui/nav_dots.h"
#include "ui/ui.h"
#include "ui/ui_internal.h"

// The features, top to bottom. Each one is defined in its own feature_*.c.
extern const ui_feature_def_t feature_images;
extern const ui_feature_def_t feature_voice;
extern const ui_feature_def_t feature_settings;

static const ui_feature_def_t *const feature_defs[] = {
    &feature_images,
    &feature_voice,
    &feature_settings,   // last, so it can list every feature's startup choices
};
#define FEATURE_COUNT       (sizeof(feature_defs) / sizeof(feature_defs[0]))
#define MAX_STARTUP_CHOICES 64

// The layout was designed for a 240x240 screen and is scaled up for bigger ones.
#define DESIGN_SIZE 240

struct ui_feature
{
    const ui_feature_def_t *def;
    lv_obj_t *row;          // this feature's tile in the up/down tile view
    lv_obj_t *pages;        // left/right tile view holding its pages
    uint8_t page_count;
};

typedef struct
{
    ui_feature_t *feature;
    lv_obj_t *page;
    const char *label;
    const char *value;
} startup_choice_t;

static lv_obj_t *features_view;     // up/down tile view
static ui_feature_t features[FEATURE_COUNT];
static uint8_t feature_count;
static startup_choice_t startup_choices[MAX_STARTUP_CHOICES];
static size_t startup_choice_count;
static int32_t screen_size;

static void setup_theme(void);
static void show_startup_page(void);
static void navigate_event_cb(lv_event_t *e);

int32_t ui_px(int32_t design_px)
{
    return design_px * screen_size / DESIGN_SIZE;
}

/********************************************************************************
function:	Build every feature that the board supports and show the startup
            page
parameter:
********************************************************************************/
void ui_init(void)
{
    setup_theme();

    features_view = lv_tileview_create(lv_screen_active());
    lv_obj_set_scrollbar_mode(features_view, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(features_view, lv_color_black(), 0);
    lv_obj_add_event_cb(features_view, navigate_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    for (size_t i = 0; i < FEATURE_COUNT; i++)
    {
        ui_feature_t *feature = &features[feature_count];
        feature->def = feature_defs[i];
        feature->row = lv_tileview_add_tile(features_view, 0, feature_count, LV_DIR_VER);
        feature->pages = lv_tileview_create(feature->row);
        lv_obj_set_scrollbar_mode(feature->pages, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_event_cb(feature->pages, navigate_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
        feature->page_count = 0;

        if (feature->def->build(feature) && feature->page_count > 0)
        {
            lv_tileview_set_tile_by_index(feature->pages, 0, 0, LV_ANIM_OFF);
            feature_count++;
        }
        else
        {
            lv_obj_delete(feature->row);
        }
    }

    nav_dots_create();      // after the features, so it's drawn over the voice ring
    show_startup_page();
    navigate_event_cb(NULL);
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
    lv_display_set_dpi(disp, ui_px(LV_DPI_DEF));

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

lv_obj_t *ui_feature_add_page(ui_feature_t *feature, bool black)
{
    lv_obj_t *page = lv_tileview_add_tile(feature->pages, feature->page_count++, 0, LV_DIR_HOR);
    if (black)
    {
        lv_obj_set_style_bg_color(page, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    }
    return page;
}

bool ui_feature_is_current(const ui_feature_t *feature)
{
    return lv_tileview_get_tile_active(features_view) == feature->row;
}

lv_obj_t *ui_current_page(void)
{
    lv_obj_t *row = lv_tileview_get_tile_active(features_view);
    for (uint8_t i = 0; i < feature_count; i++)
    {
        if (features[i].row == row)
        {
            return lv_tileview_get_tile_active(features[i].pages);
        }
    }
    return NULL;
}

void ui_add_startup_choice(ui_feature_t *feature, lv_obj_t *page, const char *label, const char *value)
{
    if (startup_choice_count < MAX_STARTUP_CHOICES)
    {
        startup_choices[startup_choice_count++] = (startup_choice_t){ feature, page, label, value };
    }
}

size_t ui_startup_choice_count(void)
{
    return startup_choice_count;
}

const char *ui_startup_choice_label(size_t index)
{
    return startup_choices[index].label;
}

const char *ui_startup_choice_value(size_t index)
{
    return startup_choices[index].value;
}

int ui_find_startup_choice(const char *value)
{
    for (size_t i = 0; i < startup_choice_count; i++)
    {
        if (strcmp(startup_choices[i].value, value) == 0)
        {
            return (int)i;
        }
    }
    return -1;
}

/********************************************************************************
function:	Show the saved startup page. Unknown values (e.g. an image that's
            since been removed) fall back to the first page.
parameter:
********************************************************************************/
static void show_startup_page(void)
{
    int index = ui_find_startup_choice(settings_get()->startup_image);
    if (index < 0)
    {
        if (startup_choice_count == 0)
        {
            return;
        }
        index = 0;
    }

    startup_choice_t *choice = &startup_choices[index];
    lv_tileview_set_tile(features_view, choice->feature->row, LV_ANIM_OFF);
    lv_tileview_set_tile(choice->feature->pages, choice->page, LV_ANIM_OFF);
}

/* A different feature or page came on screen. */
static void navigate_event_cb(lv_event_t *e)
{
    lv_obj_t *row = lv_tileview_get_tile_active(features_view);
    for (uint8_t i = 0; i < feature_count; i++)
    {
        if (features[i].row == row)
        {
            lv_obj_t *page = lv_tileview_get_tile_active(features[i].pages);
            nav_dots_show(i, feature_count, lv_obj_get_index(page), features[i].page_count);
        }
    }

    for (uint8_t i = 0; i < feature_count; i++)
    {
        if (features[i].def->on_navigate != NULL)
        {
            features[i].def->on_navigate();
        }
    }
}
