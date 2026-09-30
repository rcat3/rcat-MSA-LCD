/*
 * Settings feature: settings that apply to the whole display, one per page.
 * Settings that belong to a single feature live with that feature instead.
 */
#include <stdio.h>
#include <string.h>

#include "app/settings.h"
#include "hal.h"
#include "ui/ui_internal.h"

#define BATTERY_UPDATE_MS 1000

// Apply a new rotation only once the roller has been left alone this long, so
// flicking past options doesn't rotate the screen under your finger.
#define ROTATION_APPLY_DELAY_MS 1000

static bool build(ui_feature_t *feature);

const ui_feature_def_t feature_settings = {
    .name = "Settings",
    .build = build,
};

static lv_obj_t *battery_label;
static lv_obj_t *brightness_value_label;
static lv_timer_t *rotation_timer;
static uint8_t pending_rotation;

/* ----- Brightness (and battery voltage) ----- */

static void brightness_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *brightness_slider = lv_event_get_target_obj(e);
    uint8_t brightness = lv_slider_get_value(brightness_slider);

    lv_label_set_text_fmt(brightness_value_label, "%d%%", brightness);
    lv_obj_align_to(brightness_value_label, brightness_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, ui_px(10));
    rcat_hal_backlight_set(brightness);
    settings_set_brightness(brightness);
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

static void add_brightness_page(ui_feature_t *feature)
{
    lv_obj_t *page = ui_feature_add_page(feature, false);

    lv_obj_t *brightness_slider = lv_slider_create(page);
    lv_obj_set_size(brightness_slider, ui_px(180), ui_px(20));   // leaves room for the navigation dots
    lv_slider_set_range(brightness_slider, 1, 100);
    lv_slider_set_value(brightness_slider, settings_get()->brightness, LV_ANIM_OFF);
    lv_obj_center(brightness_slider);
    lv_obj_add_event_cb(brightness_slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    brightness_value_label = lv_label_create(page);
    lv_label_set_text_fmt(brightness_value_label, "%d%%", settings_get()->brightness);
    lv_obj_align_to(brightness_value_label, brightness_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, ui_px(10));

    lv_obj_t *brightness_label = lv_label_create(page);
    lv_label_set_text(brightness_label, "Brightness");
    lv_obj_align_to(brightness_label, brightness_slider, LV_ALIGN_OUT_TOP_MID, 0, ui_px(-10));

    battery_label = lv_label_create(page);
    lv_label_set_text(battery_label, "");
    lv_obj_align(battery_label, LV_ALIGN_TOP_MID, 0, ui_px(35));

    float volts;
    if (rcat_hal_battery_voltage(&volts))
    {
        lv_timer_t *timer = lv_timer_create(battery_timer_cb, BATTERY_UPDATE_MS, NULL);
        lv_timer_ready(timer);
    }
}

/* ----- Rotation ----- */

static void rotation_apply_timer_cb(lv_timer_t *t)
{
    lv_timer_pause(t);

    if (pending_rotation != settings_get()->rotation)
    {
        rcat_hal_display_set_rotation(pending_rotation);
        settings_set_rotation(pending_rotation);
    }
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

static void add_rotation_page(ui_feature_t *feature)
{
    lv_obj_t *page = ui_feature_add_page(feature, false);

    lv_obj_t *rotation_roller = lv_roller_create(page);
    lv_roller_set_options(rotation_roller, "0°\n90°\n180°\n270°", LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(rotation_roller, 3);
    lv_roller_set_selected(rotation_roller, settings_get()->rotation, LV_ANIM_OFF);
    lv_obj_center(rotation_roller);
    lv_obj_add_event_cb(rotation_roller, rotation_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *rotation_label = lv_label_create(page);
    lv_label_set_text(rotation_label, "Rotation");
    lv_obj_align_to(rotation_label, rotation_roller, LV_ALIGN_OUT_TOP_MID, 0, ui_px(-10));
}

/* ----- Startup page ----- */

static void startup_roller_event_cb(lv_event_t *e)
{
    lv_obj_t *startup_roller = lv_event_get_target_obj(e);
    settings_set_startup_image(ui_startup_choice_value(lv_roller_get_selected(startup_roller)));
}

static void add_startup_page(ui_feature_t *feature)
{
    lv_obj_t *page = ui_feature_add_page(feature, false);

    // Roller options: every feature's startup choices, separated by newlines.
    size_t len = 1;
    for (size_t i = 0; i < ui_startup_choice_count(); i++)
    {
        len += strlen(ui_startup_choice_label(i)) + 1;
    }
    char *options = lv_malloc(len);
    options[0] = '\0';
    for (size_t i = 0; i < ui_startup_choice_count(); i++)
    {
        if (i > 0)
        {
            strcat(options, "\n");
        }
        strcat(options, ui_startup_choice_label(i));
    }

    // Unknown values (e.g. an image that's since been removed) show the first choice.
    int selected = ui_find_startup_choice(settings_get()->startup_image);

    lv_obj_t *startup_roller = lv_roller_create(page);
    lv_roller_set_options(startup_roller, options, LV_ROLLER_MODE_NORMAL);
    lv_free(options);   // the roller keeps its own copy
    lv_roller_set_visible_row_count(startup_roller, 3);
    lv_roller_set_selected(startup_roller, (selected < 0) ? 0 : selected, LV_ANIM_OFF);
    lv_obj_center(startup_roller);
    lv_obj_add_event_cb(startup_roller, startup_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *startup_label = lv_label_create(page);
    lv_label_set_text(startup_label, "Startup");
    lv_obj_align_to(startup_label, startup_roller, LV_ALIGN_OUT_TOP_MID, 0, ui_px(-10));
}

static bool build(ui_feature_t *feature)
{
    add_brightness_page(feature);
    add_rotation_page(feature);
    add_startup_page(feature);
    return true;
}
