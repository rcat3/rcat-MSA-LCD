/*
 * Voice feature (boards with a microphone): ways of showing the microphone
 * level, one per page, then the settings page for the voice ring, the voice
 * effect, and the voice lab (boards with WiFi).
 *
 * The voice ring itself is drawn over the images feature, and over its own
 * settings page so the color can be previewed.
 */
#include <string.h>

#include "app/settings.h"
#include "audio/audio_level.h"
#include "audio/voice_fx.h"
#include "audio/voice_lab.h"
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
static lv_obj_t *lab_status_label;
static lv_obj_t *lab_switch;

#define LAB_POLL_MS 250

/********************************************************************************
function:	Show the voice ring if it's switched on and the page on screen is one
            it belongs on
parameter:
********************************************************************************/
static void on_navigate(void)
{
#if RCAT_FEATURE_IMAGES
    bool on_page = feature_images_on_screen() || ui_current_page() == ring_page;
#else
    bool on_page = ui_current_page() == ring_page;
#endif

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
    ring_page = ui_feature_add_page(feature, "Voice Ring", false);

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

/* ----- Voice effect ----- */

static void effect_roller_event_cb(lv_event_t *e)
{
    uint32_t preset = lv_roller_get_selected(lv_event_get_target_obj(e));
    voice_fx_set_preset(preset);
    settings_set_voice_effect(preset);
}

static void effect_speaker_event_cb(lv_event_t *e)
{
    voice_fx_set_speaker(lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED));
}

static void add_effect_page(ui_feature_t *feature)
{
    lv_obj_t *page = ui_feature_add_page(feature, "Voice Effect", false);

    lv_obj_t *title = lv_label_create(page);
    lv_label_set_text(title, "Voice Effect");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, ui_px(32));

    // The roller's options, one preset per line.
    static char options[256];
    options[0] = '\0';
    for (size_t i = 0; i < voice_fx_preset_count(); i++)
    {
        if (i > 0)
        {
            strncat(options, "\n", sizeof(options) - strlen(options) - 1);
        }
        strncat(options, voice_fx_preset_name(i), sizeof(options) - strlen(options) - 1);
    }

    uint8_t preset = settings_get()->voice_effect;
    if (preset >= voice_fx_preset_count())
    {
        preset = 0;
    }
    voice_fx_set_preset(preset);

    lv_obj_t *roller = lv_roller_create(page);
    lv_roller_set_options(roller, options, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(roller, 3);
    lv_roller_set_selected(roller, preset, LV_ANIM_OFF);
    lv_obj_align(roller, LV_ALIGN_CENTER, 0, ui_px(-12));
    lv_obj_add_event_cb(roller, effect_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Not saved: with the speaker where it is, it feeds back, so it's always
    // off at startup.
    lv_obj_t *speaker_label = lv_label_create(page);
    lv_label_set_text(speaker_label, "Play on speaker");
    lv_obj_align(speaker_label, LV_ALIGN_CENTER, 0, ui_px(48));

    lv_obj_t *speaker_switch = lv_switch_create(page);
    lv_obj_align(speaker_switch, LV_ALIGN_CENTER, 0, ui_px(74));
    lv_obj_add_event_cb(speaker_switch, effect_speaker_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

/* ----- Voice lab ----- */

static void lab_timer_cb(lv_timer_t *t)
{
    if (!voice_lab_running())
    {
        // Stopped by something else that needed the WiFi (the phone remote)
        lv_obj_remove_state(lab_switch, LV_STATE_CHECKED);
        lv_timer_pause(t);
    }
    voice_lab_poll(lv_tick_get());

    char text[64];
    voice_lab_status(text, sizeof(text));
    if (strcmp(lv_label_get_text(lab_status_label), text) != 0)
    {
        lv_label_set_text(lab_status_label, text);
    }
}

static void lab_switch_event_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target_obj(e);
    lv_timer_t *timer = lv_event_get_user_data(e);

    if (lv_obj_has_state(sw, LV_STATE_CHECKED))
    {
        const char *error = voice_lab_start();
        if (error != NULL)
        {
            lv_obj_remove_state(sw, LV_STATE_CHECKED);
            lv_label_set_text(lab_status_label, error);
            return;
        }
        lv_timer_resume(timer);
    }
    else
    {
        voice_lab_stop();
        lv_timer_pause(timer);
    }
    lab_timer_cb(timer);
}

static void add_lab_page(ui_feature_t *feature)
{
    lv_obj_t *page = ui_feature_add_page(feature, "Voice Lab", false);

    lv_obj_t *title = lv_label_create(page);
    lv_label_set_text(title, "Voice Lab");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, ui_px(32));

    lv_obj_t *help = lv_label_create(page);
    lv_label_set_text(help, "Streams the microphone to\nvoice_monitor.py over WiFi");
    lv_obj_set_style_text_align(help, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(help, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(help, LV_ALIGN_CENTER, 0, ui_px(-44));

    lv_timer_t *timer = lv_timer_create(lab_timer_cb, LAB_POLL_MS, NULL);
    lv_timer_pause(timer);

    lv_obj_t *sw = lv_switch_create(page);
    lab_switch = sw;
    lv_obj_align(sw, LV_ALIGN_CENTER, 0, ui_px(-6));
    lv_obj_add_event_cb(sw, lab_switch_event_cb, LV_EVENT_VALUE_CHANGED, timer);

    lab_status_label = lv_label_create(page);
    lv_label_set_text(lab_status_label, "Off");
    lv_label_set_long_mode(lab_status_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(lab_status_label, ui_px(170));
    lv_obj_set_style_text_align(lab_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lab_status_label, LV_ALIGN_CENTER, 0, ui_px(40));
}

static bool build(ui_feature_t *feature)
{
    if (!audio_level_available())
    {
        return false;
    }

    voice_ring_create();

    // Ways of showing the level. More styles can go here, one page each.
    lv_obj_t *kitt_page = ui_feature_add_page(feature, "KITT", true);
    kitt_create(kitt_page);
    ui_add_startup_choice(feature, kitt_page, "KITT", SETTINGS_STARTUP_KITT);

    add_ring_settings_page(feature);
    if (voice_fx_start())
    {
        add_effect_page(feature);
    }
    if (voice_lab_available())
    {
        add_lab_page(feature);
    }
    return true;
}
