#include <stddef.h>
#include <string.h>

#include "app/settings.h"
#include "hal.h"
#include "lvgl.h"

#define SETTINGS_MAGIC      0x54414352u     // "RCAT"
#define SETTINGS_VERSION    2               // layout of settings_record_t
#define SAVE_DELAY_MS       2000

// What actually goes into storage. `size` records how much of settings_t was
// saved, so firmware with more fields can still read older settings.
typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t checksum;      // over the first `size` bytes of values
    settings_t values;
} settings_record_t;

static settings_t current;
static settings_t saved;
static lv_timer_t *save_timer;

static void set_defaults(settings_t *s)
{
    memset(s, 0, sizeof(*s));
    s->brightness = SETTINGS_DEFAULT_BRIGHTNESS;
    s->rotation = 0;
    s->startup_image[0] = '\0';
}

static uint32_t checksum(const void *data, size_t len)
{
    // FNV-1a
    const uint8_t *p = data;
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++)
    {
        hash = (hash ^ p[i]) * 16777619u;
    }
    return hash;
}

static bool load(settings_t *out)
{
    settings_record_t rec;

    if (!rcat_hal_settings_read(&rec, sizeof(rec))
        || rec.magic != SETTINGS_MAGIC
        || rec.version != SETTINGS_VERSION
        || rec.size == 0
        || rec.size > sizeof(settings_t)
        || rec.checksum != checksum(&rec.values, rec.size))
    {
        return false;
    }

    // Start from the defaults so fields newer than the saved data keep them.
    set_defaults(out);
    memcpy(out, &rec.values, rec.size);

    // Sanity check each field on its own, so one bad value doesn't lose the rest.
    if (out->brightness < 1 || out->brightness > 100)
    {
        out->brightness = SETTINGS_DEFAULT_BRIGHTNESS;
    }
    out->rotation &= 3;
    out->startup_image[SETTINGS_NAME_LEN - 1] = '\0';
    return true;
}

void settings_init(void)
{
    if (!load(&current))
    {
        set_defaults(&current);
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
    memset(&rec, 0, sizeof(rec));
    rec.magic = SETTINGS_MAGIC;
    rec.version = SETTINGS_VERSION;
    rec.size = sizeof(settings_t);
    rec.values = current;
    rec.checksum = checksum(&rec.values, rec.size);

    if (rcat_hal_settings_write(&rec, sizeof(rec)))
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

void settings_set_startup_image(const char *name)
{
    // strncpy zero-fills the rest, which keeps the memcmp in save_timer_cb honest.
    strncpy(current.startup_image, name, SETTINGS_NAME_LEN - 1);
    current.startup_image[SETTINGS_NAME_LEN - 1] = '\0';
    schedule_save();
}
