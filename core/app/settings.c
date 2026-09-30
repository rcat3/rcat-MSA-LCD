#include <stddef.h>
#include <string.h>

#include "app/settings.h"
#include "hal.h"
#include "lvgl.h"

#define SETTINGS_MAGIC      0x54414352u     // "RCAT"
#define SETTINGS_VERSION    1
#define SAVE_DELAY_MS       2000

// What actually goes into storage. Bump SETTINGS_VERSION if settings_t
// changes in a way that old saved data can't be read as-is.
typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    settings_t values;
    uint32_t checksum;
} settings_record_t;

static const settings_t defaults = {
    .brightness = SETTINGS_DEFAULT_BRIGHTNESS,
    .rotation = 0,
};

static settings_t current;
static settings_t saved;
static lv_timer_t *save_timer;

static uint32_t checksum(const settings_record_t *rec)
{
    // FNV-1a over everything before the checksum field
    const uint8_t *p = (const uint8_t *)rec;
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < offsetof(settings_record_t, checksum); i++)
    {
        hash = (hash ^ p[i]) * 16777619u;
    }
    return hash;
}

static void fill_record(settings_record_t *rec, const settings_t *values)
{
    memset(rec, 0, sizeof(*rec));   // keep padding bytes out of the checksum
    rec->magic = SETTINGS_MAGIC;
    rec->version = SETTINGS_VERSION;
    rec->size = sizeof(settings_t);
    rec->values = *values;
    rec->checksum = checksum(rec);
}

static bool record_is_valid(const settings_record_t *rec)
{
    return rec->magic == SETTINGS_MAGIC
        && rec->version == SETTINGS_VERSION
        && rec->size == sizeof(settings_t)
        && rec->checksum == checksum(rec)
        && rec->values.brightness >= 1 && rec->values.brightness <= 100
        && rec->values.rotation <= 3;
}

void settings_init(void)
{
    settings_record_t rec;

    if (hal_settings_read(&rec, sizeof(rec)) && record_is_valid(&rec))
    {
        current = rec.values;
    }
    else
    {
        current = defaults;
    }
    saved = current;
}

const settings_t *settings_get(void)
{
    return &current;
}

static void save_timer_cb(lv_timer_t *t)
{
    lv_timer_pause(t);

    if (memcmp(&current, &saved, sizeof(current)) == 0)
    {
        return;
    }

    settings_record_t rec;
    fill_record(&rec, &current);
    if (hal_settings_write(&rec, sizeof(rec)))
    {
        saved = current;
    }
}

static void schedule_save(void)
{
    if (save_timer == NULL)
    {
        save_timer = lv_timer_create(save_timer_cb, SAVE_DELAY_MS, NULL);
    }
    lv_timer_reset(save_timer);
    lv_timer_resume(save_timer);
}

void settings_set_brightness(uint8_t percent)
{
    current.brightness = percent;
    schedule_save();
}

void settings_set_rotation(uint8_t quarter_turns)
{
    current.rotation = quarter_turns & 3;
    schedule_save();
}
