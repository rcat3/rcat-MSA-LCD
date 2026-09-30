/*
 * Voice feature (boards with a microphone): ways of showing the microphone
 * level, one per page, then the settings page for the voice ring.
 *
 * The voice ring itself is drawn over the images feature, and over its own
 * settings page so the color can be previewed.
 */
#include "app/settings.h"
#include "audio/audio_level.h"
#include "ui/kitt.h"
#include "ui/ui_internal.h"
#include "ui/voice_ring.h"

static bool build(ui_feature_t *feature);
static void on_navigate(void);

const ui_feature_def_t feature_voice = {
    .name = "Voice",
    .build = build,
    .on_navigate = on_navigate,
};

static lv_obj_t *ring_page;
static lv_obj_t *ring_switch;
static lv_obj_t *ring_roller;

/********************************************************************************
function:	Show the voice ring if it's switched on and the page on screen is one
            it belongs on
parameter:
********************************************************************************/
static void on_navigate(void)
{
    bool on_page = feature_images_on_screen() || ui_current_page() == ring_page;

    voice_ring_set_color(settings_get()->voice_ring_color);
    voice_ring_set_visible(settings_get()->voice_ring && on_page);
}

static void ring_settings_event_cb(lv_event_t *e)
{
    settings_set_voice_ring(lv_obj_has_state(ring_switch, LV_STATE_CHECKED),
                            lv_roller_get_selected(ring_roller));
    on_navigate();
}

static void add_ring_settings_page(ui_feature_t *feature)
{
    ring_page = ui_feature_add_page(feature, false);

    lv_obj_t *title = lv_label_create(ring_page);
    lv_label_set_text(title, "Voice Ring");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, ui_px(32));

    ring_switch = lv_switch_create(ring_page);
    if (settings_get()->voice_ring)
    {
        lv_obj_add_state(ring_switch, LV_STATE_CHECKED);
    }
    lv_obj_align_to(ring_switch, title, LV_ALIGN_OUT_BOTTOM_MID, 0, ui_px(8));
    lv_obj_add_event_cb(ring_switch, ring_settings_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    uint8_t color = settings_get()->voice_ring_color;
    if (color >= voice_ring_color_count())
    {
        color = 0;
    }
    ring_roller = lv_roller_create(ring_page);
    lv_roller_set_options(ring_roller, voice_ring_color_options(), LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(ring_roller, 3);
    lv_roller_set_selected(ring_roller, color, LV_ANIM_OFF);
    lv_obj_align(ring_roller, LV_ALIGN_CENTER, 0, ui_px(28));
    lv_obj_add_event_cb(ring_roller, ring_settings_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

static bool build(ui_feature_t *feature)
{
    if (!audio_level_available())
    {
        return false;
    }

    voice_ring_create();

    // Ways of showing the level. More styles can go here, one page each.
    lv_obj_t *kitt_page = ui_feature_add_page(feature, true);
    kitt_create(kitt_page);
    ui_add_startup_choice(feature, kitt_page, "KITT", SETTINGS_STARTUP_KITT);

    add_ring_settings_page(feature);
    return true;
}
