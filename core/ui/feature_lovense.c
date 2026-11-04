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
 *   Options:  Motor Hold: on, a motor keeps its level when you let go of its
 *             knob; off, it goes back to 0.
 *
 * The connections keep running while you look at other features, so the
 * levels stay as they were set.
 */
#include <stdio.h>
#include <string.h>

#include "app/lovense.h"
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
    uint8_t motor_count = d != NULL ? d->motor_count : 0;
    bool changed = slot != shown_slot || motor_count != shown_motor_count
                   || battery != shown_battery || connected != shown_connected;
    for (uint8_t i = 0; !changed && i < motor_count; i++)
    {
        changed = d->motors[i].level != shown_levels[i];
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
        lv_obj_set_hidden(grips[i], !used);
        lv_obj_set_hidden(kind_labels[i], !used);
        if (!used)
        {
            continue;
        }
        const lovense_motor_t *m = &d->motors[i];
        shown_levels[i] = m->level;
        lv_arc_set_range(rings[i], 0, m->max);
        if (lv_arc_get_value(rings[i]) != m->level)
        {
            lv_arc_set_value(rings[i], m->level);
        }
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
        if (battery >= 0)
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
    else if (settings_get()->lovense_release_to_zero)
    {
        // Let go, with Motor Hold off: back to 0
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

static lv_obj_t *add_dark_page(ui_feature_t *feature)
{
    lv_obj_t *page = ui_feature_add_page(feature, true);
    lv_obj_set_scrollable(page, false);
    lv_obj_set_style_text_color(page, lv_color_white(), 0);
    return page;
}

static void add_control_page(ui_feature_t *feature)
{
    control_page = add_dark_page(feature);

    for (uint8_t i = 0; i < LOVENSE_MAX_MOTORS; i++)
    {
        rings[i] = add_ring(control_page, i);
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
    devices_page = add_dark_page(feature);

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
    lv_obj_t *page = add_dark_page(feature);

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
    lovense_poll(lv_tick_get());
    lv_obj_t *page = ui_current_page();
    if (page == control_page)
    {
        show_control();
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
    }
}

static bool build(ui_feature_t *feature)
{
    add_control_page(feature);
    add_devices_page(feature);
    add_options_page(feature);
    show_control();
    lv_timer_create(poll_timer_cb, POLL_MS, NULL);
    return true;
}
