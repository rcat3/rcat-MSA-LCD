/*
 * Level feature (boards with an accelerometer): a spirit level for lining the
 * display up when it's mounted on the mask. A line across the screen stays
 * level with the ground, and turns green once the screen is within a degree
 * of it. It assumes the wearer is sitting or standing upright.
 */
#include <math.h>
#include <stdio.h>

#include "hal.h"
#include "ui/ui_internal.h"

#define LEVEL_UPDATE_MS     50
#define LEVEL_SMOOTHING     0.2f    // how much of each new reading to take, to steady the line
#define LEVEL_OK_DEG        1.0f    // green within this
#define LEVEL_CLOSE_DEG     3.0f    // amber within this
#define LEVEL_MIN_G         0.5f    // below this the screen is facing too far up or down to tell

static bool build(ui_feature_t *feature);
static void on_navigate(void);

const ui_feature_def_t feature_level = {
    .name = "Level",
    .build = build,
    .on_navigate = on_navigate,
};

static lv_obj_t *level_page;
static lv_obj_t *horizon;
static lv_obj_t *marks[2];
static lv_obj_t *angle_label;
static lv_timer_t *level_timer;
static lv_point_precise_t horizon_points[2];
static float up_x;
static float up_y = -1.0f;

static void set_color(lv_color_t color)
{
    lv_obj_set_style_line_color(horizon, color, 0);
    for (int i = 0; i < 2; i++)
    {
        lv_obj_set_style_bg_color(marks[i], color, 0);
    }
    lv_obj_set_style_text_color(angle_label, color, 0);
}

/********************************************************************************
function:	Read the accelerometer and turn the line to match
parameter:
********************************************************************************/
static void level_timer_cb(lv_timer_t *t)
{
    float x, y, z;
    if (!rcat_hal_accel_read(&x, &y, &z))
    {
        return;
    }

    // At rest the accelerometer points up, away from the ground.
    up_x += (x - up_x) * LEVEL_SMOOTHING;
    up_y += (y - up_y) * LEVEL_SMOOTHING;
    float g = sqrtf(up_x * up_x + up_y * up_y);
    if (g < LEVEL_MIN_G)
    {
        lv_obj_set_hidden(horizon, true);
        lv_label_set_text(angle_label, "Hold upright");
        set_color(lv_color_white());
        return;
    }

    // The line runs at right angles to "up", centered on the screen.
    float half = lv_obj_get_width(level_page) * 0.4f;
    float dx = -up_y / g * half;
    float dy = up_x / g * half;
    float cx = lv_obj_get_width(level_page) / 2.0f;
    float cy = lv_obj_get_height(level_page) / 2.0f;
    horizon_points[0] = (lv_point_precise_t){ cx - dx, cy - dy };
    horizon_points[1] = (lv_point_precise_t){ cx + dx, cy + dy };
    lv_line_set_points(horizon, horizon_points, 2);
    lv_obj_set_hidden(horizon, false);

    float degrees = fabsf(atan2f(up_x, -up_y)) * 180.0f / (float)M_PI;
    char text[16];
    snprintf(text, sizeof(text), "%.1f°", degrees);
    lv_label_set_text(angle_label, text);

    if (degrees <= LEVEL_OK_DEG)
    {
        set_color(lv_palette_main(LV_PALETTE_GREEN));
    }
    else if (degrees <= LEVEL_CLOSE_DEG)
    {
        set_color(lv_palette_main(LV_PALETTE_AMBER));
    }
    else
    {
        set_color(lv_color_white());
    }
}

/* Only read the accelerometer while the page is on screen. */
static void on_navigate(void)
{
    if (ui_current_page() == level_page)
    {
        lv_timer_resume(level_timer);
        lv_timer_ready(level_timer);
    }
    else
    {
        lv_timer_pause(level_timer);
    }
}

static bool build(ui_feature_t *feature)
{
    float x, y, z;
    if (!rcat_hal_accel_read(&x, &y, &z))
    {
        return false;
    }
    up_x = x;
    up_y = y;

    level_page = ui_feature_add_page(feature, "Level", true);
    lv_obj_set_scrollable(level_page, false);

    // Marks at each side show where the line sits when the screen is level.
    for (int i = 0; i < 2; i++)
    {
        marks[i] = lv_obj_create(level_page);
        lv_obj_remove_style_all(marks[i]);
        lv_obj_set_size(marks[i], ui_px(16), ui_px(4));
        lv_obj_set_style_bg_opa(marks[i], LV_OPA_COVER, 0);
        lv_obj_align(marks[i], i == 0 ? LV_ALIGN_LEFT_MID : LV_ALIGN_RIGHT_MID, i == 0 ? ui_px(12) : ui_px(-12), 0);
    }

    horizon = lv_line_create(level_page);
    lv_obj_set_style_line_width(horizon, ui_px(4), 0);
    lv_obj_set_style_line_rounded(horizon, true, 0);

    angle_label = lv_label_create(level_page);
    lv_label_set_text(angle_label, "");
    lv_obj_align(angle_label, LV_ALIGN_CENTER, 0, ui_px(40));

    lv_obj_t *title = lv_label_create(level_page);
    lv_label_set_text(title, "Level");
    lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, ui_px(32));

    set_color(lv_color_white());
    level_timer = lv_timer_create(level_timer_cb, LEVEL_UPDATE_MS, NULL);
    lv_timer_pause(level_timer);

    ui_add_startup_choice(feature, level_page, "Level", "level");
    return true;
}
