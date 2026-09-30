/*
 * Navigation dots: after each swipe, a row of dots at the bottom edge shows
 * which page of the feature you're on, and a column of dots at the right
 * edge shows which feature. They fade out after a moment.
 */
#include <stdio.h>

#include "ui/nav_dots.h"
#include "ui/ui_internal.h"

#define DOT_SIZE        6       // in 240x240 design pixels
#define ACTIVE_DOT_SIZE 8
#define DOT_GAP         5
#define EDGE_MARGIN     10
#define MAX_DOTS        12      // more pages than this show "3 / 34" instead
#define SHOW_MS         1500
#define FADE_MS         400

typedef struct
{
    lv_obj_t *pill;
    uint8_t count;
    uint8_t active;
} dots_t;

static dots_t page_dots;
static dots_t feature_dots;

static lv_obj_t *create_pill(lv_flex_flow_t flow, lv_align_t align, int32_t x, int32_t y)
{
    lv_obj_t *pill = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(pill);
    lv_obj_set_clickable(pill, false);
    lv_obj_set_scrollable(pill, false);
    lv_obj_set_size(pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(pill, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_50, 0);
    lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(pill, ui_px(4), 0);
    lv_obj_set_style_pad_gap(pill, ui_px(DOT_GAP), 0);
    lv_obj_set_flex_flow(pill, flow);
    lv_obj_set_flex_align(pill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(pill, align, x, y);
    lv_obj_set_hidden(pill, true);
    return pill;
}

static void style_dot(lv_obj_t *dot, bool active)
{
    int32_t size = LV_MAX(3, ui_px(active ? ACTIVE_DOT_SIZE : DOT_SIZE));
    lv_obj_set_size(dot, size, size);
    lv_obj_set_style_bg_opa(dot, active ? LV_OPA_COVER : LV_OPA_40, 0);
}

/* Show `count` dots with `active` highlighted, rebuilding them only when the
 * count changes. */
static void update_dots(dots_t *dots, uint8_t count, uint8_t active)
{
    if (count != dots->count)
    {
        lv_obj_clean(dots->pill);
        if (count > MAX_DOTS)
        {
            lv_obj_t *label = lv_label_create(dots->pill);
            lv_obj_set_style_text_color(label, lv_color_white(), 0);
            lv_obj_set_style_pad_hor(label, ui_px(4), 0);
        }
        else
        {
            for (uint8_t i = 0; i < count; i++)
            {
                lv_obj_t *dot = lv_obj_create(dots->pill);
                lv_obj_remove_style_all(dot);
                lv_obj_set_clickable(dot, false);
                lv_obj_set_style_bg_color(dot, lv_color_white(), 0);
                lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
                style_dot(dot, false);
            }
        }
        dots->count = count;
        dots->active = UINT8_MAX;
    }

    if (count > MAX_DOTS)
    {
        lv_label_set_text_fmt(lv_obj_get_child(dots->pill, 0), "%d / %d", active + 1, count);
    }
    else if (active != dots->active)
    {
        if (dots->active < count)
        {
            style_dot(lv_obj_get_child(dots->pill, dots->active), false);
        }
        style_dot(lv_obj_get_child(dots->pill, active), true);
    }
    dots->active = active;
}

/* Show a pill, then fade it out. A single dot isn't worth showing. */
static void flash(dots_t *dots)
{
    lv_anim_delete(dots->pill, NULL);
    lv_obj_set_hidden(dots->pill, dots->count <= 1);
    lv_obj_set_style_opa(dots->pill, LV_OPA_COVER, 0);
    lv_obj_fade_out(dots->pill, FADE_MS, SHOW_MS);
}

void nav_dots_create(void)
{
    page_dots.pill = create_pill(LV_FLEX_FLOW_ROW, LV_ALIGN_BOTTOM_MID, 0, -ui_px(EDGE_MARGIN));
    feature_dots.pill = create_pill(LV_FLEX_FLOW_COLUMN, LV_ALIGN_RIGHT_MID, -ui_px(EDGE_MARGIN), 0);
}

void nav_dots_show(uint8_t feature, uint8_t feature_count, uint8_t page, uint8_t page_count)
{
    update_dots(&feature_dots, feature_count, feature);
    update_dots(&page_dots, page_count, page);
    flash(&feature_dots);
    flash(&page_dots);
}
