/*
 * Settings feature: settings that apply to the whole display, one per page.
 * Settings that belong to a single feature live with that feature instead.
 *
 * On boards with WiFi, the last page is the phone remote (see app/remote.h):
 * a switch for its hotspot, the QR code that joins it, and a new password.
 */
#include <stdio.h>
#include <string.h>

#include "app/remote.h"
#include "app/settings.h"
#include "hal.h"
#include "ui/ui_internal.h"

#define BATTERY_UPDATE_MS 1000
#define REMOTE_POLL_MS    50
#define COLOR_MUTED       lv_palette_main(LV_PALETTE_GREY)

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
    lv_obj_t *page = ui_feature_add_page(feature, "Brightness", false);

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
    lv_obj_t *page = ui_feature_add_page(feature, "Rotation", false);

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
    lv_obj_t *page = ui_feature_add_page(feature, "Startup", false);

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

/* ----- Phone remote ----- */

#if RCAT_HAVE_NETWORK

static lv_obj_t *remote_page;
static lv_obj_t *remote_title;
static lv_obj_t *remote_switch;
static lv_obj_t *remote_qr;
static lv_obj_t *remote_hint;
static lv_obj_t *remote_name;
static lv_obj_t *remote_pass;
static lv_obj_t *remote_new;    // new password, while it's off
static int shown_phones = -1;
static int shown_remote = -1;
static char shown_join[80];

static void show_remote(void)
{
    bool on = remote_running();
    if ((int)on != shown_remote)
    {
        shown_remote = on;
        lv_obj_set_state(remote_switch, LV_STATE_CHECKED, on);
        lv_obj_set_hidden(remote_qr, !on);
        lv_obj_set_hidden(remote_name, !on);
        lv_obj_set_hidden(remote_pass, !on);
        lv_obj_set_hidden(remote_hint, on);
        lv_obj_set_hidden(remote_new, on);
        shown_phones = -1;
    }
    if (on && strcmp(remote_join_text(), shown_join) != 0)
    {
        // A new password, or the first time it's shown
        snprintf(shown_join, sizeof(shown_join), "%s", remote_join_text());
        lv_qrcode_update(remote_qr, shown_join, strlen(shown_join));
        lv_label_set_text(remote_name, remote_ssid());
        lv_label_set_text_fmt(remote_pass, "Password  %s", remote_password());
    }
    int phones = remote_phones();
    if (phones != shown_phones)
    {
        shown_phones = phones;
        if (!on)
        {
            lv_label_set_text(remote_title, "Phone Remote");
        }
        else if (phones == 0)
        {
            lv_label_set_text(remote_title, "Scan to join");
        }
        else
        {
            lv_label_set_text_fmt(remote_title, LV_SYMBOL_WIFI "  %d phone%s", phones, phones == 1 ? "" : "s");
        }
    }
}

static void remote_switch_event_cb(lv_event_t *e)
{
    bool on = lv_obj_has_state(remote_switch, LV_STATE_CHECKED);
    if (on && !remote_start())
    {
        lv_label_set_text(remote_hint, "WiFi didn't start");
    }
    else if (!on)
    {
        remote_stop();
        lv_label_set_text(remote_hint, "Control the display\nfrom a phone");
    }
    shown_remote = -1;
    show_remote();
}

static void new_password_event_cb(lv_event_t *e)
{
    remote_new_password();
    lv_label_set_text_fmt(remote_hint, "New password:\n%s", remote_password());
}

/* The remote runs from here, whatever's on screen. */
static void remote_timer_cb(lv_timer_t *t)
{
    remote_poll(lv_tick_get());
    if (ui_current_page() == remote_page)
    {
        show_remote();
    }
}

static void add_remote_page(ui_feature_t *feature)
{
    remote_page = ui_feature_add_page(feature, "Phone Remote", false);
    lv_obj_set_scrollable(remote_page, false);

    remote_title = lv_label_create(remote_page);
    lv_label_set_text(remote_title, "Phone Remote");
    lv_obj_align(remote_title, LV_ALIGN_TOP_MID, 0, ui_px(28));

    remote_switch = lv_switch_create(remote_page);
    lv_obj_set_size(remote_switch, ui_px(64), ui_px(32));
    lv_obj_align(remote_switch, LV_ALIGN_CENTER, 0, ui_px(-58));
    lv_obj_add_event_cb(remote_switch, remote_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // For a phone's camera: "join this network"
    remote_qr = lv_qrcode_create(remote_page);
    lv_qrcode_set_size(remote_qr, ui_px(92));
    lv_qrcode_set_dark_color(remote_qr, lv_color_black());
    lv_qrcode_set_light_color(remote_qr, lv_color_white());
    lv_qrcode_set_quiet_zone(remote_qr, true);
    lv_obj_align(remote_qr, LV_ALIGN_CENTER, 0, ui_px(8));

    remote_hint = lv_label_create(remote_page);
    lv_label_set_text(remote_hint, "Control the display\nfrom a phone");
    lv_obj_set_style_text_align(remote_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(remote_hint, COLOR_MUTED, 0);
    lv_obj_align(remote_hint, LV_ALIGN_CENTER, 0, ui_px(0));

    // Shuts out anyone who had the old one
    remote_new = lv_button_create(remote_page);
    lv_obj_set_size(remote_new, ui_px(130), ui_px(32));
    lv_obj_set_style_radius(remote_new, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(remote_new, LV_ALIGN_CENTER, 0, ui_px(52));
    lv_obj_add_event_cb(remote_new, new_password_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *text = lv_label_create(remote_new);
    lv_label_set_text(text, "New password");
    lv_obj_center(text);

    remote_name = lv_label_create(remote_page);
    lv_label_set_text(remote_name, "");
    lv_obj_align(remote_name, LV_ALIGN_CENTER, 0, ui_px(66));

    remote_pass = lv_label_create(remote_page);
    lv_label_set_text(remote_pass, "");
    lv_obj_set_style_text_color(remote_pass, COLOR_MUTED, 0);
    lv_obj_align(remote_pass, LV_ALIGN_CENTER, 0, ui_px(84));

    show_remote();
    lv_timer_create(remote_timer_cb, REMOTE_POLL_MS, NULL);
}

#endif  // RCAT_HAVE_NETWORK

static bool build(ui_feature_t *feature)
{
    add_brightness_page(feature);
    add_rotation_page(feature);
    add_startup_page(feature);
#if RCAT_HAVE_NETWORK
    if (remote_available())
    {
        add_remote_page(feature);
    }
#endif
    return true;
}
