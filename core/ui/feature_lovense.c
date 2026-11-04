/*
 * Lovense feature (boards with Bluetooth): controls up to three Lovense
 * devices at once, on dark pages.
 *
 *   Control:  a ring around the screen for each of the device's motors, with
 *             the level on its knob. Only the knobs can be dragged, so swipes
 *             anywhere else still move between pages. The device's name is in
 *             the middle, with arrows to step through the connected devices,
 *             and a Stop button under it that stops them all as soon as it's
 *             touched.
 *   Devices:  looks for devices while it's on screen. Tap one to connect to
 *             it, or tap it again to stop it and disconnect.
 *   Patterns: plays a pattern on one device or all of them. While one plays,
 *             each ring on the control page sets how high that motor goes,
 *             and a thin ring inside it shows the level as it plays. The
 *             speed is under the list.
 *   React:    drives the motors from your voice or your movement (see
 *             reactive.h), on top of any pattern.
 *   Options:  Motor Hold: on, a motor keeps its level when you let go of its
 *             knob; off, it goes back to 0.
 *
 * The connections keep running while you look at other features, so the
 * levels stay as they were set. Everything here can also be done from a
 * phone, with the phone remote in Settings (see remote.h).
 */
#include <stdio.h>
#include <string.h>

#include "app/lovense.h"
#include "app/patterns.h"
#include "app/reactive.h"
#include "app/settings.h"
#include "ui/ui_internal.h"

#define POLL_MS             50
#define NOTICE_MS           2500    // how long "Only 3 at once" and the like stay up

// Ring layout, in 240x240 design pixels. The rings sit inside one another,
// open at the bottom, each with a knob that's bigger than the ring.
#define RING_OUTER          220     // outer ring's diameter
#define RING_STEP           48      // each ring is this much smaller across
#define RING_WIDTH          12
#define KNOB_PAD            5       // how far a knob sticks out past its ring
#define KNOB_REACH          18      // a touch this close to a knob grabs it
#define RING_START          135     // degrees, clockwise from 3 o'clock
#define RING_SWEEP          270

// Dark theme colors
#define COLOR_TRACK         lv_color_hex(0x262626)
#define COLOR_SURFACE       lv_color_hex(0x1c1c1c)
#define COLOR_OUTLINE       lv_color_hex(0x3a3a3a)
#define COLOR_MUTED         lv_color_hex(0x8a8a8a)
#define COLOR_CONNECTED     lv_palette_main(LV_PALETTE_GREEN)

static bool build(ui_feature_t *feature);
static void on_navigate(void);

const ui_feature_def_t feature_lovense = {
    .name = "Lovense",
    .build = build,
    .on_navigate = on_navigate,
};

static const lv_palette_t ring_colors[LOVENSE_MAX_MOTORS] = {
    LV_PALETTE_PINK, LV_PALETTE_PURPLE, LV_PALETTE_CYAN,
};

/* ----- Control page ----- */

static lv_obj_t *control_page;
static lv_obj_t *rings[LOVENSE_MAX_MOTORS];
static lv_obj_t *live_rings[LOVENSE_MAX_MOTORS];   // thin, inside each ring: the level while a pattern plays
static lv_obj_t *grips[LOVENSE_MAX_MOTORS];        // invisible, over each knob, to drag it
static lv_obj_t *knob_labels[LOVENSE_MAX_MOTORS];  // the level, on the grip
static lv_obj_t *kind_labels[LOVENSE_MAX_MOTORS];
static lv_obj_t *name_label;
static lv_obj_t *prev_button;
static lv_obj_t *next_button;
static lv_obj_t *battery_label;
static lv_obj_t *stop_button;
static lv_obj_t *nothing_label;

static int selected = -1;           // slot shown on the control page, or -1

// What the control page last showed, to redraw only on a change
static int shown_slot = -2;
static uint8_t shown_motor_count;
static int shown_battery = -2;
static int shown_connected = -1;
static uint8_t shown_levels[LOVENSE_MAX_MOTORS];
static uint8_t shown_caps[LOVENSE_MAX_MOTORS];
static int shown_pattern = -2;
static bool shown_driven;
static char shown_status[40];

/* ----- Devices page ----- */

static lv_obj_t *devices_page;
static lv_obj_t *device_rows[LOVENSE_MAX_DEVICES];
static lv_obj_t *device_row_labels[LOVENSE_MAX_DEVICES];
static lv_obj_t *devices_status;
static char row_texts[LOVENSE_MAX_DEVICES][48];
static uint32_t notice_until;
static bool no_bluetooth;

static bool is_connected(int slot)
{
    const lovense_device_t *d = slot >= 0 ? lovense_device(slot) : NULL;
    return d != NULL && d->state == LOVENSE_CONNECTED;
}

static int count_connected(void)
{
    int n = 0;
    for (int i = 0; i < LOVENSE_MAX_DEVICES; i++)
    {
        n += is_connected(i);
    }
    return n;
}

/* The next connected device after slot, going forwards (step 1) or back
 * (step -1) and wrapping round, or -1. */
static int step_connected(int slot, int step)
{
    for (int n = 1; n <= LOVENSE_MAX_DEVICES; n++)
    {
        int i = ((slot + step * n) % LOVENSE_MAX_DEVICES + LOVENSE_MAX_DEVICES) % LOVENSE_MAX_DEVICES;
        if (is_connected(i))
        {
            return i;
        }
    }
    return -1;
}

/* "Vibrate", or "Vibrate 2" when the device has more than one of a kind. */
static void motor_title(const lovense_device_t *d, uint8_t motor, char *out, size_t len)
{
    lovense_motor_kind_t kind = d->motors[motor].kind;
    int same = 0, index = 0;
    for (uint8_t i = 0; i < d->motor_count; i++)
    {
        if (d->motors[i].kind == kind)
        {
            same++;
            if (i <= motor)
            {
                index++;
            }
        }
    }
    if (same > 1)
    {
        snprintf(out, len, "%s %d", lovense_motor_name(kind), index);
    }
    else
    {
        snprintf(out, len, "%s", lovense_motor_name(kind));
    }
}

/* ----- Ring geometry ----- */

static int32_t ring_mid_radius(uint8_t i)
{
    return ui_px(RING_OUTER - RING_STEP * i) / 2 - ui_px(RING_WIDTH) / 2;
}

/* Where ring i's knob is, as an offset from the screen's center. */
static void knob_offset(uint8_t i, int32_t *x, int32_t *y)
{
    int32_t max = lv_arc_get_max_value(rings[i]);
    int32_t value = lv_arc_get_value(rings[i]);
    int32_t angle = RING_START + (max > 0 ? RING_SWEEP * value / max : 0);
    int32_t r = ring_mid_radius(i);
    *x = (r * lv_trigo_cos(angle)) >> LV_TRIGO_SHIFT;
    *y = (r * lv_trigo_sin(angle)) >> LV_TRIGO_SHIFT;
}

/* The level for a touch at p (screen coordinates) on ring i. In the gap at
 * the bottom it sticks to whichever end is nearer, so dragging past an end
 * doesn't jump to the other one. */
static int32_t level_at(uint8_t i, const lv_point_t *p)
{
    lv_area_t a;
    lv_obj_get_coords(rings[i], &a);
    int32_t dx = p->x - (a.x1 + a.x2) / 2;
    int32_t dy = p->y - (a.y1 + a.y2) / 2;
    int32_t max = lv_arc_get_max_value(rings[i]);
    if (dx == 0 && dy == 0)
    {
        return lv_arc_get_value(rings[i]);
    }
    int32_t angle = ((int32_t)lv_atan2(dy, dx) - RING_START + 720) % 360;   // 0 at the ring's start
    if (angle > RING_SWEEP)
    {
        return angle < RING_SWEEP + (360 - RING_SWEEP) / 2 ? max : 0;
    }
    return (angle * max + RING_SWEEP / 2) / RING_SWEEP;
}

/* ----- Control page ----- */

/* Move ring i's grip (and the level on it) to its knob. */
static void place_knob_label(uint8_t i)
{
    int32_t x, y;
    knob_offset(i, &x, &y);
    lv_label_set_text_fmt(knob_labels[i], "%d", (int)lv_arc_get_value(rings[i]));
    lv_obj_align(grips[i], LV_ALIGN_CENTER, x, y);
}

/* Lay out the middle for the space the rings leave. */
static void layout_middle(uint8_t count)
{
    int32_t inner = ui_px(RING_OUTER - RING_STEP * (count > 0 ? count - 1 : 0)) / 2 - ui_px(RING_WIDTH + KNOB_PAD + 4);
    int32_t stop_size = inner * 85 / 100;
    lv_obj_set_size(stop_button, stop_size, stop_size);
    lv_obj_align(stop_button, LV_ALIGN_CENTER, 0, inner * 30 / 100);

    int32_t name_y = -inner * 52 / 100;
    int32_t arrow = ui_px(count >= 3 ? 20 : 26);
    lv_obj_set_width(name_label, inner * 2 - 2 * arrow - ui_px(4));
    lv_obj_align(name_label, LV_ALIGN_CENTER, 0, name_y);
    lv_obj_set_size(prev_button, arrow, arrow);
    lv_obj_set_size(next_button, arrow, arrow);
    lv_obj_align(prev_button, LV_ALIGN_CENTER, -(inner - arrow / 2 - ui_px(2)), name_y);
    lv_obj_align(next_button, LV_ALIGN_CENTER, inner - arrow / 2 - ui_px(2), name_y);
    lv_obj_align(battery_label, LV_ALIGN_CENTER, 0, name_y + ui_px(17));
    // There's no room for the battery inside three rings.
    lv_obj_set_hidden(battery_label, count >= 3);
}

static void show_control(void)
{
    // Keep to the device picked on the devices page while it connects;
    // otherwise move on to another one if the one shown has gone.
    if (!is_connected(selected))
    {
        const lovense_device_t *d = selected >= 0 ? lovense_device(selected) : NULL;
        int next = step_connected(selected < 0 ? 0 : selected, 1);
        if (next >= 0 || d == NULL || d->state != LOVENSE_CONNECTING)
        {
            selected = next;
        }
    }
    const lovense_device_t *d = is_connected(selected) ? lovense_device(selected) : NULL;

    int connected = count_connected();
    int slot = d != NULL ? selected : -1;
    int battery = d != NULL ? d->battery : -1;
    int pattern = d != NULL ? d->pattern : -1;
    bool driven = d != NULL && lovense_driven(d);
    char status[40] = "";
    if (d != NULL && lovense_reactive())
    {
        reactive_status(status, sizeof(status));
    }
    uint8_t motor_count = d != NULL ? d->motor_count : 0;
    bool changed = slot != shown_slot || motor_count != shown_motor_count || battery != shown_battery
                   || connected != shown_connected || pattern != shown_pattern || driven != shown_driven
                   || strcmp(status, shown_status) != 0;
    for (uint8_t i = 0; !changed && i < motor_count; i++)
    {
        changed = d->motors[i].level != shown_levels[i] || d->motors[i].cap != shown_caps[i];
    }
    if (!changed)
    {
        return;
    }
    bool relayout = slot != shown_slot || motor_count != shown_motor_count;
    shown_slot = slot;
    shown_motor_count = motor_count;
    shown_battery = battery;
    shown_connected = connected;
    shown_pattern = pattern;
    shown_driven = driven;
    strcpy(shown_status, status);

    lv_obj_set_hidden(nothing_label, d != NULL);
    lv_obj_set_hidden(name_label, d == NULL);
    lv_obj_set_hidden(stop_button, d == NULL);
    lv_obj_set_hidden(prev_button, d == NULL || connected < 2);
    lv_obj_set_hidden(next_button, d == NULL || connected < 2);
    if (d == NULL)
    {
        lv_obj_set_hidden(battery_label, true);
    }

    for (uint8_t i = 0; i < LOVENSE_MAX_MOTORS; i++)
    {
        bool used = i < motor_count;
        lv_obj_set_hidden(rings[i], !used);
        lv_obj_set_hidden(live_rings[i], !used || !driven);
        lv_obj_set_hidden(grips[i], !used);
        lv_obj_set_hidden(kind_labels[i], !used);
        if (!used)
        {
            continue;
        }
        // While a pattern or reaction drives it, the ring (dimmed) and its
        // knob set how high it goes, and the thin ring inside shows the level.
        const lovense_motor_t *m = &d->motors[i];
        shown_levels[i] = m->level;
        shown_caps[i] = m->cap;
        int32_t value = driven ? m->cap : m->level;
        lv_arc_set_range(rings[i], 0, m->max);
        if (lv_arc_get_value(rings[i]) != value)
        {
            lv_arc_set_value(rings[i], value);
        }
        lv_obj_set_style_arc_opa(rings[i], driven ? LV_OPA_40 : LV_OPA_COVER, LV_PART_INDICATOR);
        lv_arc_set_range(live_rings[i], 0, m->max);
        lv_arc_set_value(live_rings[i], m->level);
        place_knob_label(i);
        if (relayout)
        {
            char title[16];
            motor_title(d, i, title, sizeof(title));
            lv_label_set_text(kind_labels[i], title);
            // At the ring's 0 end, inside the gap at the bottom
            int32_t r = ring_mid_radius(i);
            int32_t x = (r * lv_trigo_cos(RING_START)) >> LV_TRIGO_SHIFT;
            int32_t y = (r * lv_trigo_sin(RING_START)) >> LV_TRIGO_SHIFT;
            lv_obj_align(kind_labels[i], LV_ALIGN_LEFT_MID, ui_px(120) + x + ui_px(14), y);
        }
    }

    if (d != NULL)
    {
        if (relayout)
        {
            layout_middle(motor_count);
        }
        lv_label_set_text(name_label, d->model);
        if (status[0] != '\0')
        {
            lv_label_set_text(battery_label, status);
        }
        else if (pattern >= 0)
        {
            lv_label_set_text_fmt(battery_label, LV_SYMBOL_PLAY " %s", patterns_name((size_t)pattern));
        }
        else if (battery >= 0)
        {
            lv_label_set_text_fmt(battery_label, LV_SYMBOL_BATTERY_FULL " %d%%", battery);
        }
        else
        {
            lv_label_set_text(battery_label, "");
        }
    }
}

static void grip_event_cb(lv_event_t *e)
{
    uint8_t motor = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    if (!is_connected(selected))
    {
        return;
    }
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSING)
    {
        lv_point_t p;
        lv_indev_get_point(lv_indev_active(), &p);
        int32_t level = level_at(motor, &p);
        if (level == lv_arc_get_value(rings[motor]))
        {
            return;
        }
        lv_arc_set_value(rings[motor], level);
        lovense_set_level(selected, motor, level);
    }
    else if (settings_get()->lovense_release_to_zero && !lovense_driven(lovense_device(selected)))
    {
        // Let go, with Motor Hold off: back to 0 (but a pattern or reaction keeps its cap)
        lv_arc_set_value(rings[motor], 0);
        lovense_set_level(selected, motor, 0);
    }
    place_knob_label(motor);
    show_control();
}

static void step_event_cb(lv_event_t *e)
{
    int step = (int)(intptr_t)lv_event_get_user_data(e);
    int next = step_connected(selected, step);
    if (next >= 0)
    {
        selected = next;
    }
    show_control();
}

static void stop_event_cb(lv_event_t *e)
{
    lovense_stop_all();
    reactive_set_mode(REACTIVE_OFF);
    show_control();
}

/* A round, dark button that doesn't pass drags on to the page, so a finger
 * that moves a little still counts as a press. */
static lv_obj_t *add_round_button(lv_obj_t *page, const char *text, lv_color_t color)
{
    lv_obj_t *button = lv_button_create(page);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(button, color, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_scroll_chain(button, false);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return button;
}

static lv_obj_t *add_ring(lv_obj_t *page, uint8_t index)
{
    lv_color_t color = lv_palette_main(ring_colors[index]);
    int32_t size = ui_px(RING_OUTER - RING_STEP * index);
    lv_obj_t *arc = lv_arc_create(page);
    lv_obj_set_size(arc, size, size);
    lv_arc_set_bg_angles(arc, RING_START, (RING_START + RING_SWEEP) % 360);
    lv_arc_set_value(arc, 0);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, ui_px(RING_WIDTH), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, ui_px(RING_WIDTH), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, COLOR_TRACK, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(arc, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, ui_px(KNOB_PAD), LV_PART_KNOB);
    lv_obj_set_style_shadow_width(arc, ui_px(8), LV_PART_KNOB);
    lv_obj_set_style_shadow_color(arc, color, LV_PART_KNOB);
    lv_obj_set_style_shadow_opa(arc, LV_OPA_60, LV_PART_KNOB);
    lv_obj_center(arc);
    // The ring only shows the level. Its grip (below) is what's dragged, so
    // swipes that start anywhere else move between pages.
    lv_obj_set_clickable(arc, false);
    return arc;
}

/* A thin ring along the middle of ring index, showing only its level. */
static lv_obj_t *add_live_ring(lv_obj_t *page, uint8_t index)
{
    int32_t inset = (ui_px(RING_WIDTH) - ui_px(4)) / 2;
    int32_t size = ui_px(RING_OUTER - RING_STEP * index) - 2 * inset;
    lv_obj_t *arc = lv_arc_create(page);
    lv_obj_set_size(arc, size, size);
    lv_arc_set_bg_angles(arc, RING_START, (RING_START + RING_SWEEP) % 360);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, ui_px(4), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_palette_lighten(ring_colors[index], 3), LV_PART_INDICATOR);
    lv_obj_set_style_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_center(arc);
    lv_obj_set_clickable(arc, false);
    lv_obj_set_hidden(arc, true);
    return arc;
}

static lv_obj_t *add_dark_page(ui_feature_t *feature, const char *name)
{
    lv_obj_t *page = ui_feature_add_page(feature, name, true);
    lv_obj_set_scrollable(page, false);
    lv_obj_set_style_text_color(page, lv_color_white(), 0);
    return page;
}

static void add_control_page(ui_feature_t *feature)
{
    control_page = add_dark_page(feature, "Control");

    for (uint8_t i = 0; i < LOVENSE_MAX_MOTORS; i++)
    {
        rings[i] = add_ring(control_page, i);
        live_rings[i] = add_live_ring(control_page, i);
        kind_labels[i] = lv_label_create(control_page);
        lv_obj_set_style_text_color(kind_labels[i], lv_palette_lighten(ring_colors[i], 1), 0);
    }
    // A grip over each knob, over all the rings, with the level on it. It
    // doesn't pass drags on to the page, so dragging it only turns the ring.
    for (uint8_t i = 0; i < LOVENSE_MAX_MOTORS; i++)
    {
        grips[i] = lv_obj_create(control_page);
        lv_obj_remove_style_all(grips[i]);
        lv_obj_set_size(grips[i], ui_px(2 * KNOB_REACH), ui_px(2 * KNOB_REACH));
        lv_obj_set_scrollable(grips[i], false);
        lv_obj_set_scroll_chain(grips[i], false);
        lv_obj_add_event_cb(grips[i], grip_event_cb, LV_EVENT_PRESSING, (void *)(intptr_t)i);
        lv_obj_add_event_cb(grips[i], grip_event_cb, LV_EVENT_RELEASED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(grips[i], grip_event_cb, LV_EVENT_PRESS_LOST, (void *)(intptr_t)i);
        knob_labels[i] = lv_label_create(grips[i]);
        lv_obj_set_style_text_color(knob_labels[i], lv_color_black(), 0);
        lv_obj_center(knob_labels[i]);
    }

    nothing_label = lv_label_create(control_page);
    lv_label_set_text(nothing_label, "Nothing connected\n\nSwipe left to\nfind a device");
    lv_obj_set_style_text_align(nothing_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(nothing_label, COLOR_MUTED, 0);
    lv_obj_center(nothing_label);

    name_label = lv_label_create(control_page);
    lv_label_set_long_mode(name_label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(name_label, LV_TEXT_ALIGN_CENTER, 0);

    prev_button = add_round_button(control_page, LV_SYMBOL_LEFT, COLOR_SURFACE);
    lv_obj_add_event_cb(prev_button, step_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
    next_button = add_round_button(control_page, LV_SYMBOL_RIGHT, COLOR_SURFACE);
    lv_obj_add_event_cb(next_button, step_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);

    battery_label = lv_label_create(control_page);
    lv_obj_set_style_text_color(battery_label, COLOR_MUTED, 0);

    // Stops on the touch itself rather than the release, so it can't be
    // lost to a swipe.
    stop_button = add_round_button(control_page, "STOP", lv_palette_darken(LV_PALETTE_RED, 1));
    lv_obj_add_event_cb(stop_button, stop_event_cb, LV_EVENT_PRESSED, NULL);
}

/* ----- Patterns page ----- */

static lv_obj_t *patterns_page;
static lv_obj_t *target_label;
static lv_obj_t *target_prev;
static lv_obj_t *target_next;
static lv_obj_t **pattern_rows;
static lv_obj_t **pattern_row_labels;
static int8_t *pattern_row_shown;      // -1 unknown, 0 stopped, 1 playing
static size_t pattern_row_count;
static lv_obj_t *speed_label;
static int target = -1;                // the device patterns play on, or -1 for all of them
static int shown_target = -2;
static int shown_target_connected = -1;
static uint32_t pattern_notice_until;
static int shown_speed = -1;

/* Whether pattern p is playing on the target: on it, or on every
 * connected device when the target is all of them. */
static bool playing_on_target(int p)
{
    if (target >= 0)
    {
        return is_connected(target) && lovense_device(target)->pattern == p;
    }
    bool any = false;
    for (int i = 0; i < LOVENSE_MAX_DEVICES; i++)
    {
        if (is_connected(i))
        {
            if (lovense_device(i)->pattern != p)
            {
                return false;
            }
            any = true;
        }
    }
    return any;
}

static void show_patterns(void)
{
    int connected = count_connected();
    if (target >= 0 && !is_connected(target))
    {
        target = -1;
    }
    if ((target != shown_target || connected != shown_target_connected)
        && (int32_t)(lv_tick_get() - pattern_notice_until) >= 0)
    {
        shown_target = target;
        shown_target_connected = connected;
        if (connected == 0)
        {
            lv_label_set_text(target_label, "Nothing connected");
        }
        else if (target >= 0)
        {
            lv_label_set_text(target_label, lovense_device(target)->model);
        }
        else
        {
            lv_label_set_text(target_label, connected == 1 ? "Connected device" : "All devices");
        }
        lv_obj_set_hidden(target_prev, connected < 2);
        lv_obj_set_hidden(target_next, connected < 2);
    }

    for (size_t i = 0; i < pattern_row_count; i++)
    {
        int8_t playing = playing_on_target((int)i);
        if (playing == pattern_row_shown[i])
        {
            continue;
        }
        pattern_row_shown[i] = playing;
        if (playing)
        {
            lv_label_set_text_fmt(pattern_row_labels[i], LV_SYMBOL_PLAY "  %s", patterns_name(i));
        }
        else
        {
            lv_label_set_text(pattern_row_labels[i], patterns_name(i));
        }
        lv_color_t color = lv_palette_main(LV_PALETTE_PINK);
        lv_obj_set_style_border_color(pattern_rows[i], playing ? color : COLOR_OUTLINE, 0);
        lv_obj_set_style_text_color(pattern_row_labels[i], playing ? color : lv_color_white(), 0);
    }
    if (lovense_speed() != shown_speed)
    {
        shown_speed = lovense_speed();
        lv_label_set_text_fmt(speed_label, "Speed %d%%", shown_speed);
    }
}

static void pattern_row_event_cb(lv_event_t *e)
{
    int p = (int)(intptr_t)lv_event_get_user_data(e);
    if (count_connected() == 0)
    {
        lv_label_set_text(target_label, "Connect one first");
        pattern_notice_until = lv_tick_get() + NOTICE_MS;
        shown_target = -2;
        return;
    }
    int play = playing_on_target(p) ? -1 : p;
    for (int i = 0; i < LOVENSE_MAX_DEVICES; i++)
    {
        if (is_connected(i) && (target < 0 || target == i))
        {
            lovense_play(i, play);
        }
    }
    show_patterns();
}

/* All, then each connected device, and round again. */
static void target_event_cb(lv_event_t *e)
{
    int step = (int)(intptr_t)lv_event_get_user_data(e);
    int choices[LOVENSE_MAX_DEVICES + 1];
    int count = 0, at = 0;
    choices[count++] = -1;
    for (int i = 0; i < LOVENSE_MAX_DEVICES; i++)
    {
        if (is_connected(i))
        {
            if (i == target)
            {
                at = count;
            }
            choices[count++] = i;
        }
    }
    target = choices[(at + step + count) % count];
    for (size_t i = 0; i < pattern_row_count; i++)
    {
        pattern_row_shown[i] = -1;
    }
    show_patterns();
}

static void speed_event_cb(lv_event_t *e)
{
    int step = (int)(intptr_t)lv_event_get_user_data(e);
    int speed = (int)lovense_speed() + step * 25;
    lovense_set_speed((uint16_t)LV_CLAMP(25, speed, 300));
    show_patterns();
}

static void add_patterns_page(ui_feature_t *feature)
{
    patterns_page = add_dark_page(feature, "Patterns");

    // Which device: at the top, with arrows to step through them
    target_label = lv_label_create(patterns_page);
    lv_label_set_long_mode(target_label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(target_label, ui_px(110));
    lv_obj_set_style_text_align(target_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(target_label, COLOR_MUTED, 0);
    lv_obj_align(target_label, LV_ALIGN_TOP_MID, 0, ui_px(30));
    target_prev = add_round_button(patterns_page, LV_SYMBOL_LEFT, COLOR_SURFACE);
    lv_obj_set_size(target_prev, ui_px(24), ui_px(24));
    lv_obj_align(target_prev, LV_ALIGN_TOP_MID, ui_px(-70), ui_px(26));
    lv_obj_add_event_cb(target_prev, target_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
    target_next = add_round_button(patterns_page, LV_SYMBOL_RIGHT, COLOR_SURFACE);
    lv_obj_set_size(target_next, ui_px(24), ui_px(24));
    lv_obj_align(target_next, LV_ALIGN_TOP_MID, ui_px(70), ui_px(26));
    lv_obj_add_event_cb(target_next, target_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);

    lv_obj_t *list = lv_obj_create(patterns_page);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, ui_px(180), ui_px(116));
    lv_obj_align(list, LV_ALIGN_CENTER, 0, ui_px(-6));
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list, ui_px(8), 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

    pattern_row_count = patterns_count();
    pattern_rows = lv_malloc(pattern_row_count * sizeof(lv_obj_t *));
    pattern_row_labels = lv_malloc(pattern_row_count * sizeof(lv_obj_t *));
    pattern_row_shown = lv_malloc(pattern_row_count);
    if (pattern_rows == NULL || pattern_row_labels == NULL || pattern_row_shown == NULL)
    {
        pattern_row_count = 0;
    }
    for (size_t i = 0; i < pattern_row_count; i++)
    {
        lv_obj_t *row = lv_button_create(list);
        lv_obj_set_size(row, ui_px(176), ui_px(30));
        lv_obj_set_style_radius(row, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(row, COLOR_SURFACE, 0);
        lv_obj_set_style_bg_color(row, COLOR_OUTLINE, LV_STATE_PRESSED);
        lv_obj_set_style_border_width(row, LV_MAX(1, ui_px(1)), 0);
        lv_obj_set_style_border_color(row, COLOR_OUTLINE, 0);
        lv_obj_set_style_shadow_width(row, 0, 0);
        lv_obj_add_event_cb(row, pattern_row_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *label = lv_label_create(row);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_width(label, ui_px(156));
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(label);
        pattern_rows[i] = row;
        pattern_row_labels[i] = label;
        pattern_row_shown[i] = -1;
    }

    // Speed, under the list
    lv_obj_t *slower = add_round_button(patterns_page, LV_SYMBOL_MINUS, COLOR_SURFACE);
    lv_obj_set_size(slower, ui_px(28), ui_px(28));
    lv_obj_align(slower, LV_ALIGN_CENTER, ui_px(-62), ui_px(76));
    lv_obj_add_event_cb(slower, speed_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
    lv_obj_t *faster = add_round_button(patterns_page, LV_SYMBOL_PLUS, COLOR_SURFACE);
    lv_obj_set_size(faster, ui_px(28), ui_px(28));
    lv_obj_align(faster, LV_ALIGN_CENTER, ui_px(62), ui_px(76));
    lv_obj_add_event_cb(faster, speed_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);
    speed_label = lv_label_create(patterns_page);
    lv_obj_set_style_text_color(speed_label, COLOR_MUTED, 0);
    lv_obj_align(speed_label, LV_ALIGN_CENTER, 0, ui_px(76));
}

/* ----- React page ----- */

static lv_obj_t *react_page;
static lv_obj_t *react_rows[REACTIVE_COUNT];
static lv_obj_t *react_row_labels[REACTIVE_COUNT];
static lv_obj_t *react_status;
static int shown_react_mode = -1;
static char shown_react_status[40];

static void show_react(void)
{
    reactive_mode_t mode = reactive_mode();
    if ((int)mode != shown_react_mode)
    {
        shown_react_mode = mode;
        for (int i = 0; i < REACTIVE_COUNT; i++)
        {
            if (react_rows[i] == NULL)
            {
                continue;
            }
            bool on = i == (int)mode;
            lv_color_t color = lv_palette_main(LV_PALETTE_PINK);
            lv_obj_set_style_border_color(react_rows[i], on ? color : COLOR_OUTLINE, 0);
            lv_obj_set_style_text_color(react_row_labels[i], on ? color : lv_color_white(), 0);
        }
    }
    char status[40];
    if (mode == REACTIVE_OFF)
    {
        snprintf(status, sizeof(status), "%s", count_connected() > 0 ? "" : "Connect a device first");
    }
    else
    {
        reactive_status(status, sizeof(status));
    }
    if (strcmp(status, shown_react_status) != 0)
    {
        strcpy(shown_react_status, status);
        lv_label_set_text(react_status, status);
    }
}

static void react_row_event_cb(lv_event_t *e)
{
    reactive_mode_t mode = (reactive_mode_t)(intptr_t)lv_event_get_user_data(e);
    if (mode == reactive_mode())
    {
        mode = REACTIVE_OFF;    // tapping the one that's on turns it off
    }
    reactive_set_mode(mode);
    lovense_set_reactive(mode != REACTIVE_OFF);
    show_react();
}

static void add_react_page(ui_feature_t *feature)
{
    react_page = add_dark_page(feature, "React");

    lv_obj_t *title = lv_label_create(react_page);
    lv_label_set_text(title, "React to");
    lv_obj_set_style_text_color(title, COLOR_MUTED, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, ui_px(28));

    lv_obj_t *list = lv_obj_create(react_page);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, ui_px(180), ui_px(130));
    lv_obj_align(list, LV_ALIGN_CENTER, 0, ui_px(-2));
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list, ui_px(8), 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

    // Off isn't listed: tapping the one that's on turns it off.
    for (int i = REACTIVE_OFF + 1; i < REACTIVE_COUNT; i++)
    {
        if (!reactive_available((reactive_mode_t)i))
        {
            continue;   // no microphone, or no accelerometer
        }
        lv_obj_t *row = lv_button_create(list);
        lv_obj_set_size(row, ui_px(176), ui_px(30));
        lv_obj_set_style_radius(row, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(row, COLOR_SURFACE, 0);
        lv_obj_set_style_bg_color(row, COLOR_OUTLINE, LV_STATE_PRESSED);
        lv_obj_set_style_border_width(row, LV_MAX(1, ui_px(1)), 0);
        lv_obj_set_style_border_color(row, COLOR_OUTLINE, 0);
        lv_obj_set_style_shadow_width(row, 0, 0);
        lv_obj_add_event_cb(row, react_row_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *label = lv_label_create(row);
        lv_label_set_text(label, reactive_name((reactive_mode_t)i));
        lv_obj_center(label);
        react_rows[i] = row;
        react_row_labels[i] = label;
    }

    react_status = lv_label_create(react_page);
    lv_label_set_text(react_status, "");
    lv_obj_set_style_text_color(react_status, COLOR_MUTED, 0);
    lv_obj_align(react_status, LV_ALIGN_CENTER, 0, ui_px(84));
}

/* ----- Devices page ----- */

static void show_notice(const char *text)
{
    lv_label_set_text(devices_status, text);
    notice_until = lv_tick_get() + NOTICE_MS;
}

static void show_devices(void)
{
    bool any = false;
    for (size_t i = 0; i < LOVENSE_MAX_DEVICES; i++)
    {
        const lovense_device_t *d = lovense_device(i);
        lv_obj_set_hidden(device_rows[i], d == NULL);
        if (d == NULL)
        {
            row_texts[i][0] = '\0';
            continue;
        }
        any = true;

        char text[sizeof(row_texts[0])];
        switch (d->state)
        {
        case LOVENSE_CONNECTING:
            snprintf(text, sizeof(text), "%s  ...", d->name);
            break;
        case LOVENSE_CONNECTED:
            if (d->battery >= 0)
            {
                snprintf(text, sizeof(text), LV_SYMBOL_OK "  %s  %d%%", d->model, d->battery);
            }
            else
            {
                snprintf(text, sizeof(text), LV_SYMBOL_OK "  %s", d->model);
            }
            break;
        case LOVENSE_FAILED:
            snprintf(text, sizeof(text), "%s  failed", d->name);
            break;
        case LOVENSE_LOST:
            snprintf(text, sizeof(text), "%s  lost", d->name);
            break;
        default:
            snprintf(text, sizeof(text), "%s", d->name);
            break;
        }
        if (strcmp(text, row_texts[i]) != 0)
        {
            strcpy(row_texts[i], text);
            lv_label_set_text(device_row_labels[i], text);
            bool on = d->state == LOVENSE_CONNECTED;
            lv_obj_set_style_border_color(device_rows[i], on ? COLOR_CONNECTED : COLOR_OUTLINE, 0);
            lv_obj_set_style_text_color(device_row_labels[i], on ? COLOR_CONNECTED : lv_color_white(), 0);
        }
    }

    if ((int32_t)(lv_tick_get() - notice_until) < 0)
    {
        return;     // a notice is still up
    }
    if (no_bluetooth)
    {
        lv_label_set_text(devices_status, "No Bluetooth");
    }
    else if (lovense_scanning())
    {
        lv_label_set_text(devices_status, any ? "Tap to connect" : "Searching...");
    }
    else
    {
        lv_label_set_text(devices_status, "");
    }
}

static void device_row_event_cb(lv_event_t *e)
{
    size_t slot = (size_t)(intptr_t)lv_event_get_user_data(e);
    const lovense_device_t *d = lovense_device(slot);
    if (d == NULL)
    {
        return;
    }
    bool connecting = d->state != LOVENSE_CONNECTING && d->state != LOVENSE_CONNECTED;
    if (!lovense_toggle(slot))
    {
        if (connecting)
        {
            show_notice(count_connected() >= LOVENSE_MAX_CONNECTED ? "Only 3 at once" : "Wait for the other one");
        }
    }
    else if (connecting)
    {
        selected = (int)slot;   // show it on the control page once it's connected
    }
    show_devices();
}

static void add_devices_page(ui_feature_t *feature)
{
    devices_page = add_dark_page(feature, "Devices");

    lv_obj_t *title = lv_label_create(devices_page);
    lv_label_set_text(title, "Devices");
    lv_obj_set_style_text_color(title, COLOR_MUTED, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, ui_px(28));

    lv_obj_t *list = lv_obj_create(devices_page);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, ui_px(180), ui_px(136));
    lv_obj_align(list, LV_ALIGN_CENTER, 0, ui_px(-2));
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list, ui_px(8), 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

    for (size_t i = 0; i < LOVENSE_MAX_DEVICES; i++)
    {
        lv_obj_t *row = lv_button_create(list);
        lv_obj_set_size(row, ui_px(176), ui_px(30));
        lv_obj_set_style_radius(row, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(row, COLOR_SURFACE, 0);
        lv_obj_set_style_bg_color(row, COLOR_OUTLINE, LV_STATE_PRESSED);
        lv_obj_set_style_border_width(row, LV_MAX(1, ui_px(1)), 0);
        lv_obj_set_style_border_color(row, COLOR_OUTLINE, 0);
        lv_obj_set_style_shadow_width(row, 0, 0);
        lv_obj_add_event_cb(row, device_row_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        device_row_labels[i] = lv_label_create(row);
        lv_label_set_long_mode(device_row_labels[i], LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_width(device_row_labels[i], ui_px(156));
        lv_obj_set_style_text_align(device_row_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(device_row_labels[i]);
        lv_obj_set_hidden(row, true);
        device_rows[i] = row;
    }

    devices_status = lv_label_create(devices_page);
    lv_label_set_text(devices_status, "");
    lv_obj_set_style_text_color(devices_status, COLOR_MUTED, 0);
    lv_obj_align(devices_status, LV_ALIGN_CENTER, 0, ui_px(84));
}

/* ----- Options page ----- */

static void hold_event_cb(lv_event_t *e)
{
    bool hold = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
    settings_set_lovense_release_to_zero(!hold);
}

static void add_options_page(ui_feature_t *feature)
{
    lv_obj_t *page = add_dark_page(feature, "Options");

    lv_obj_t *label = lv_label_create(page);
    lv_label_set_text(label, "Motor Hold");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, ui_px(-34));

    lv_obj_t *hold_switch = lv_switch_create(page);
    lv_obj_set_size(hold_switch, ui_px(96), ui_px(48));
    lv_obj_set_style_bg_color(hold_switch, COLOR_TRACK, LV_PART_MAIN);
    lv_obj_set_style_bg_color(hold_switch, lv_palette_main(LV_PALETTE_PINK), LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (!settings_get()->lovense_release_to_zero)
    {
        lv_obj_add_state(hold_switch, LV_STATE_CHECKED);
    }
    lv_obj_align(hold_switch, LV_ALIGN_CENTER, 0, ui_px(12));
    lv_obj_add_event_cb(hold_switch, hold_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

/* ----- Running it ----- */

static void poll_timer_cb(lv_timer_t *t)
{
    uint32_t now = lv_tick_get();
    if (reactive_mode() != REACTIVE_OFF)
    {
        lovense_set_reactive_factor(reactive_poll(now));
    }
    lovense_poll(now);
    lv_obj_t *page = ui_current_page();
    if (page == control_page)
    {
        show_control();
    }
    else if (page == patterns_page)
    {
        show_patterns();
    }
    else if (page == react_page)
    {
        show_react();
    }
    else if (page == devices_page)
    {
        show_devices();
    }
}

/* Look for devices only while the devices page is on screen. */
static void on_navigate(void)
{
    if (ui_current_page() == devices_page)
    {
        if (!lovense_scanning() && !lovense_scan_start())
        {
            no_bluetooth = true;
        }
        show_devices();
    }
    else
    {
        lovense_scan_stop();
        if (ui_current_page() == control_page)
        {
            shown_slot = -2;    // redraw it all
            show_control();
        }
        else if (ui_current_page() == patterns_page)
        {
            shown_target = -2;
            show_patterns();
        }
    }
}

static bool build(ui_feature_t *feature)
{
    add_control_page(feature);
    add_devices_page(feature);
    add_patterns_page(feature);
    add_react_page(feature);
    add_options_page(feature);
    show_control();
    lv_timer_create(poll_timer_cb, POLL_MS, NULL);
    return true;
}
