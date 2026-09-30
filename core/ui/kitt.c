/*
 * KITT style voice box: three columns of red segments that light up from the
 * middle outwards with the microphone level. The middle column is taller and
 * reacts a little more than the side ones.
 */
#include <stdlib.h>

#include "audio/audio_level.h"
#include "ui/kitt.h"
#include "ui/ui_internal.h"

// Layout, in 240x240 design pixels
#define COLUMNS             3
#define CENTER_SEGMENTS     15      // odd, so there's a middle segment
#define SIDE_SEGMENTS       11
#define SEGMENT_WIDTH       44
#define SEGMENT_HEIGHT      7
#define SEGMENT_PITCH       10      // height plus the gap below it
#define COLUMN_SPACING      60

#define SIDE_LEVEL_PERCENT  80      // side columns show a bit less than the middle
#define UPDATE_MS           30

#define COLOR_LIT           0xff2010
#define COLOR_UNLIT         0x2a0404

static const uint8_t column_segments[COLUMNS] = { SIDE_SEGMENTS, CENTER_SEGMENTS, SIDE_SEGMENTS };
static lv_obj_t *segments[COLUMNS][CENTER_SEGMENTS];

// Lit segments per column: 0 is none, 1 is just the middle one, 2 adds the
// ones above and below it, and so on.
static uint8_t lit_steps[COLUMNS];

static void set_lit_steps(int column, uint8_t steps)
{
    int count = column_segments[column];
    int middle = count / 2;

    for (int i = 0; i < count; i++)
    {
        bool lit = abs(i - middle) < steps;
        lv_obj_set_style_bg_color(segments[column][i], lv_color_hex(lit ? COLOR_LIT : COLOR_UNLIT), 0);
    }
    lit_steps[column] = steps;
}

static void update_timer_cb(lv_timer_t *t)
{
    // Nothing to do while the tile is scrolled off screen.
    if (!lv_obj_is_visible(segments[1][CENTER_SEGMENTS / 2]))
    {
        return;
    }

    uint32_t level = audio_level_get();

    for (int column = 0; column < COLUMNS; column++)
    {
        uint32_t column_level = (column == 1) ? level : level * SIDE_LEVEL_PERCENT / 100;
        uint32_t max_steps = column_segments[column] / 2 + 1;
        uint32_t steps = (column_level * max_steps + AUDIO_LEVEL_MAX / 2) / AUDIO_LEVEL_MAX;

        if (steps > max_steps)
        {
            steps = max_steps;
        }
        if (steps != lit_steps[column])
        {
            set_lit_steps(column, steps);
        }
    }
}

void kitt_create(lv_obj_t *parent)
{
    for (int column = 0; column < COLUMNS; column++)
    {
        int count = column_segments[column];
        int32_t x = (column - 1) * ui_px(COLUMN_SPACING);

        for (int i = 0; i < count; i++)
        {
            lv_obj_t *segment = lv_obj_create(parent);
            lv_obj_remove_style_all(segment);
            lv_obj_remove_flag(segment, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_size(segment, ui_px(SEGMENT_WIDTH), ui_px(SEGMENT_HEIGHT));
            lv_obj_set_style_bg_opa(segment, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(segment, lv_color_hex(COLOR_UNLIT), 0);
            lv_obj_set_style_radius(segment, ui_px(1), 0);
            lv_obj_align(segment, LV_ALIGN_CENTER, x, (i - count / 2) * ui_px(SEGMENT_PITCH));
            segments[column][i] = segment;
        }
    }

    lv_timer_create(update_timer_cb, UPDATE_MS, NULL);
}
