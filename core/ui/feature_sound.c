/*
 * Sound feature (boards with a speaker): a player for the sound files on the
 * SD card, then a page of sound options. Sound keeps playing in the
 * background while you look at other pages.
 */
#include <string.h>

#include "app/settings.h"
#include "audio/audio_player.h"
#include "ui/ui_internal.h"

// How often the player page catches up with the speaker task (the track
// changes at the end of each one).
#define STATUS_UPDATE_MS 250

static bool build(ui_feature_t *feature);

const ui_feature_def_t feature_sound = {
    .name = "Sound",
    .build = build,
};

static lv_obj_t *track_label;
static lv_obj_t *position_label;
static lv_obj_t *play_label;
static lv_obj_t *volume_value_label;
static lv_obj_t *repeat_label;

static const char *const repeat_texts[AUDIO_REPEAT_COUNT] = {
    LV_SYMBOL_LOOP "  Repeat all",
    LV_SYMBOL_LOOP "  Repeat one",
    LV_SYMBOL_STOP "  Play once",
};

/* The track saved in the settings, which holds the start of its name. */
static int find_saved_track(void)
{
    const char *saved = settings_get()->sound_track;
    size_t len = strlen(saved);
    if (len == 0)
    {
        return -1;
    }
    for (size_t i = 0; i < audio_player_track_count(); i++)
    {
        const char *name = audio_player_track_name(i);
        if (strncmp(name, saved, len) == 0 && (name[len] == '\0' || len == SETTINGS_NAME_LEN - 1))
        {
            return (int)i;
        }
    }
    return -1;
}

/* ----- Player page ----- */

static void show_status(void)
{
    size_t index = audio_player_current();
    const char *name = audio_player_track_name(index);

    if (strcmp(lv_label_get_text(track_label), name) != 0)
    {
        lv_label_set_text(track_label, name);
        lv_label_set_text_fmt(position_label, "%d / %d", (int)index + 1, (int)audio_player_track_count());
        settings_set_sound_track(name);
    }
    lv_label_set_text(play_label, audio_player_is_playing() ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
}

static void status_timer_cb(lv_timer_t *t)
{
    show_status();
}

static void play_event_cb(lv_event_t *e)
{
    if (audio_player_is_playing())
    {
        audio_player_pause();
    }
    else
    {
        audio_player_resume();
    }
    show_status();
}

static void skip_event_cb(lv_event_t *e)
{
    int step = (int)(intptr_t)lv_event_get_user_data(e);
    size_t count = audio_player_track_count();
    size_t index = (audio_player_current() + count + step) % count;
    if (audio_player_is_playing())
    {
        audio_player_play(index);
    }
    else
    {
        audio_player_cue(index);
    }
    show_status();
}

static lv_obj_t *add_button(lv_obj_t *page, const char *symbol, int32_t size, int32_t x)
{
    lv_obj_t *button = lv_button_create(page);
    lv_obj_set_size(button, ui_px(size), ui_px(size));
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(button, LV_ALIGN_CENTER, ui_px(x), ui_px(20));

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, symbol);
    lv_obj_center(label);
    return button;
}

static void add_player_page(ui_feature_t *feature)
{
    lv_obj_t *page = ui_feature_add_page(feature, false);

    lv_obj_t *title = lv_label_create(page);
    lv_label_set_text(title, "Sound");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, ui_px(32));

    if (audio_player_track_count() == 0)
    {
        lv_obj_t *help = lv_label_create(page);
        lv_label_set_text(help, "Put WAV, MP3 or FLAC files in a folder called \"sounds\" on the SD card.");
        lv_label_set_long_mode(help, LV_LABEL_LONG_MODE_WRAP);
        lv_obj_set_width(help, ui_px(170));
        lv_obj_set_style_text_align(help, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(help);
        return;
    }

    track_label = lv_label_create(page);
    lv_label_set_text(track_label, "");
    lv_label_set_long_mode(track_label, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_set_width(track_label, ui_px(170));
    lv_obj_set_style_text_align(track_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(track_label, LV_ALIGN_CENTER, 0, ui_px(-42));

    position_label = lv_label_create(page);
    lv_label_set_text(position_label, "");
    lv_obj_set_style_text_color(position_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(position_label, LV_ALIGN_CENTER, 0, ui_px(-18));

    lv_obj_t *prev = add_button(page, LV_SYMBOL_PREV, 44, -62);
    lv_obj_add_event_cb(prev, skip_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)-1);

    lv_obj_t *play = add_button(page, LV_SYMBOL_PLAY, 58, 0);
    play_label = lv_obj_get_child(play, 0);
    lv_obj_add_event_cb(play, play_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *next = add_button(page, LV_SYMBOL_NEXT, 44, 62);
    lv_obj_add_event_cb(next, skip_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);

    show_status();
    lv_timer_create(status_timer_cb, STATUS_UPDATE_MS, NULL);
}

/* ----- Options page ----- */

static void volume_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target_obj(e);
    uint8_t volume = lv_slider_get_value(slider);

    lv_label_set_text_fmt(volume_value_label, "%d%%", volume);
    audio_player_set_volume(volume);
    settings_set_sound_volume(volume);
}

static void repeat_event_cb(lv_event_t *e)
{
    uint8_t repeat = (settings_get()->sound_repeat + 1) % AUDIO_REPEAT_COUNT;
    lv_label_set_text(repeat_label, repeat_texts[repeat]);
    audio_player_set_repeat(repeat);
    settings_set_sound_repeat(repeat);
}

static void autoplay_event_cb(lv_event_t *e)
{
    settings_set_sound_autoplay(lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED));
}

static void add_options_page(ui_feature_t *feature)
{
    lv_obj_t *page = ui_feature_add_page(feature, false);

    lv_obj_t *volume_label = lv_label_create(page);
    lv_label_set_text(volume_label, "Volume");
    lv_obj_align(volume_label, LV_ALIGN_TOP_MID, 0, ui_px(32));

    lv_obj_t *slider = lv_slider_create(page);
    lv_obj_set_size(slider, ui_px(160), ui_px(20));
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, settings_get()->sound_volume, LV_ANIM_OFF);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, ui_px(-56));
    lv_obj_add_event_cb(slider, volume_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    volume_value_label = lv_label_create(page);
    lv_label_set_text_fmt(volume_value_label, "%d%%", settings_get()->sound_volume);
    lv_obj_align(volume_value_label, LV_ALIGN_CENTER, 0, ui_px(-34));

    lv_obj_t *repeat_button = lv_button_create(page);
    lv_obj_align(repeat_button, LV_ALIGN_CENTER, 0, ui_px(4));
    lv_obj_add_event_cb(repeat_button, repeat_event_cb, LV_EVENT_CLICKED, NULL);
    repeat_label = lv_label_create(repeat_button);
    lv_label_set_text(repeat_label, repeat_texts[settings_get()->sound_repeat]);
    lv_obj_center(repeat_label);

    lv_obj_t *autoplay_label = lv_label_create(page);
    lv_label_set_text(autoplay_label, "Play at startup");
    lv_obj_align(autoplay_label, LV_ALIGN_CENTER, 0, ui_px(42));

    lv_obj_t *autoplay_switch = lv_switch_create(page);
    if (settings_get()->sound_autoplay)
    {
        lv_obj_add_state(autoplay_switch, LV_STATE_CHECKED);
    }
    lv_obj_align(autoplay_switch, LV_ALIGN_CENTER, 0, ui_px(68));
    lv_obj_add_event_cb(autoplay_switch, autoplay_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

static bool build(ui_feature_t *feature)
{
    if (!audio_player_available())
    {
        return false;
    }

    if (settings_get()->sound_repeat >= AUDIO_REPEAT_COUNT)
    {
        settings_set_sound_repeat(AUDIO_REPEAT_ALL);
    }
    audio_player_set_volume(settings_get()->sound_volume);
    audio_player_set_repeat(settings_get()->sound_repeat);

    // Pick up where it left off: the last track played, and playing it
    // straight away if Play at startup is on.
    int saved = find_saved_track();
    size_t first = (saved >= 0) ? (size_t)saved : 0;
    if (settings_get()->sound_autoplay)
    {
        audio_player_play(first);
    }
    else
    {
        audio_player_cue(first);
    }

    add_player_page(feature);
    add_options_page(feature);
    return true;
}
