/*
 * Voice ring: a ring around the edge of the round screen that gets brighter
 * with the microphone level.
 *
 * The ring is drawn by hand on a full screen object on the top layer. When
 * the brightness changes, only a band of small rectangles around the edge is
 * redrawn instead of the whole screen, so it doesn't slow down whatever is
 * showing underneath.
 */
#include <math.h>
#include <string.h>

#include "audio/audio_level.h"
#include "ui/ui_internal.h"
#include "ui/voice_ring.h"

#define RING_WIDTH          10      // in 240x240 design pixels
#define UPDATE_MS           30
#define INVALIDATE_SECTIONS 32      // pieces of the ring redrawn separately

typedef struct
{
    const char *name;
    uint32_t rgb;
} ring_color_t;

static const ring_color_t colors[] = {
    { "Red",    0xff2010 },
    { "Orange", 0xff7a00 },
    { "Yellow", 0xffd400 },
    { "Green",  0x20e040 },
    { "Cyan",   0x00d8ff },
    { "Blue",   0x2060ff },
    { "Purple", 0x9a40ff },
    { "Pink",   0xff40c0 },
    { "White",  0xffffff },
};
#define COLOR_COUNT (sizeof(colors) / sizeof(colors[0]))

static lv_obj_t *ring;
static lv_color_t ring_color;
static lv_opa_t ring_opa;

static void ring_geometry(lv_point_t *center, int32_t *outer_radius, int32_t *width)
{
    lv_area_t coords;
    lv_obj_get_coords(ring, &coords);
    center->x = (coords.x1 + coords.x2) / 2;
    center->y = (coords.y1 + coords.y2) / 2;
    *outer_radius = LV_MIN(lv_area_get_width(&coords), lv_area_get_height(&coords)) / 2;
    *width = ui_px(RING_WIDTH);
}

static void draw_event_cb(lv_event_t *e)
{
    if (ring_opa == LV_OPA_TRANSP)
    {
        return;
    }

    lv_point_t center;
    int32_t radius;
    int32_t width;
    ring_geometry(&center, &radius, &width);

    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.center = center;
    dsc.radius = radius;
    dsc.width = width;
    dsc.start_angle = 0;
    dsc.end_angle = 360;
    dsc.color = ring_color;
    dsc.opa = ring_opa;
    lv_draw_arc(lv_event_get_layer(e), &dsc);
}

/* Redraw just the band around the edge where the ring is. */
static void invalidate_ring(void)
{
    lv_point_t center;
    int32_t radius;
    int32_t width;
    ring_geometry(&center, &radius, &width);

    // Each section's box covers its corners, plus a margin for the curve
    // bulging out between them.
    const int32_t margin = 3;
    int32_t inner = radius - width;

    for (int i = 0; i < INVALIDATE_SECTIONS; i++)
    {
        float a0 = 2.0f * (float)M_PI * i / INVALIDATE_SECTIONS;
        float a1 = 2.0f * (float)M_PI * (i + 1) / INVALIDATE_SECTIONS;
        float xs[4] = { radius * cosf(a0), radius * cosf(a1), inner * cosf(a0), inner * cosf(a1) };
        float ys[4] = { radius * sinf(a0), radius * sinf(a1), inner * sinf(a0), inner * sinf(a1) };

        float x_min = xs[0], x_max = xs[0], y_min = ys[0], y_max = ys[0];
        for (int k = 1; k < 4; k++)
        {
            x_min = fminf(x_min, xs[k]);
            x_max = fmaxf(x_max, xs[k]);
            y_min = fminf(y_min, ys[k]);
            y_max = fmaxf(y_max, ys[k]);
        }

        lv_area_t area = {
            .x1 = center.x + (int32_t)floorf(x_min) - margin,
            .y1 = center.y + (int32_t)floorf(y_min) - margin,
            .x2 = center.x + (int32_t)ceilf(x_max) + margin,
            .y2 = center.y + (int32_t)ceilf(y_max) + margin,
        };
        lv_obj_invalidate_area(ring, &area);
    }
}

static void update_timer_cb(lv_timer_t *t)
{
    lv_opa_t opa = LV_OPA_TRANSP;

    if (!lv_obj_has_flag(ring, LV_OBJ_FLAG_HIDDEN))
    {
        opa = (lv_opa_t)(audio_level_get() * LV_OPA_COVER / AUDIO_LEVEL_MAX);
    }

    if (opa != ring_opa)
    {
        ring_opa = opa;
        invalidate_ring();
    }
}

void voice_ring_create(void)
{
    ring = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(ring);
    lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ring, LV_PCT(100), LV_PCT(100));
    lv_obj_add_event_cb(ring, draw_event_cb, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_flag(ring, LV_OBJ_FLAG_HIDDEN);

    ring_color = lv_color_hex(colors[0].rgb);
    lv_timer_create(update_timer_cb, UPDATE_MS, NULL);
}

void voice_ring_set_visible(bool visible)
{
    if (visible)
    {
        lv_obj_remove_flag(ring, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(ring, LV_OBJ_FLAG_HIDDEN);
        ring_opa = LV_OPA_TRANSP;
    }
}

void voice_ring_set_color(uint8_t color_index)
{
    if (color_index >= COLOR_COUNT)
    {
        color_index = 0;
    }
    ring_color = lv_color_hex(colors[color_index].rgb);
    invalidate_ring();
}

const char *voice_ring_color_options(void)
{
    static char options[128];

    if (options[0] == '\0')
    {
        for (size_t i = 0; i < COLOR_COUNT; i++)
        {
            if (i > 0)
            {
                strcat(options, "\n");
            }
            strcat(options, colors[i].name);
        }
    }
    return options;
}

uint8_t voice_ring_color_count(void)
{
    return COLOR_COUNT;
}
