/*
 * Terminal feature: shows what's typed on the phone remote (see
 * app/message.h) like an old green screen terminal. The text types itself
 * out a character at a time behind a block cursor, which blinks once it's
 * done, in a blocky pixel font, with faint scan lines over it all.
 *
 * The letter size is a setting (settings_t.terminal_size), chosen on the
 * phone: one of six sizes, or the biggest the whole message fits at, but no
 * smaller than AUTO_MIN_SCALE, to stay readable. When it doesn't fit, it
 * scrolls like a terminal: the newest lines stay on screen and the oldest go
 * off the top.
 */
#include <string.h>

#include "app/message.h"
#include "app/settings.h"
#include "ui/ui_internal.h"

#define COLOR_PHOSPHOR      lv_color_hex(0x33ff33)
#define TICK_MS             40
#define BLINK_MS            530
#define BOX_DESIGN_PX       168     // the text fits in a square this big, inside the round screen
#define SCANLINE_PERIOD     3       // a dark line every this many rows
#define SCANLINE_OPA        90      // and how dark (0 to 255)
#define PROMPT              ">"

// Fonts from tools/gen_terminal_font.py, 8 * scale px wide
#define MAX_SCALE           6
#define AUTO_MIN_SCALE      2       // fitting the message goes no smaller than 16 px
LV_FONT_DECLARE(terminal_font_1)
LV_FONT_DECLARE(terminal_font_2)
LV_FONT_DECLARE(terminal_font_3)
LV_FONT_DECLARE(terminal_font_4)
LV_FONT_DECLARE(terminal_font_5)
LV_FONT_DECLARE(terminal_font_6)
static const lv_font_t *const fonts[] = {
    NULL, &terminal_font_1, &terminal_font_2, &terminal_font_3, &terminal_font_4, &terminal_font_5, &terminal_font_6,
};

static bool build(ui_feature_t *feature);
static void on_navigate(void);

const ui_feature_def_t feature_terminal = {
    .name = "Terminal",
    .build = build,
    .on_navigate = on_navigate,
};

static lv_obj_t *page;
static lv_obj_t *box;           // holds the text and the cursor, centred
static lv_obj_t *label;
static lv_obj_t *cursor;
static lv_image_dsc_t scanlines;

static char wrapped[MESSAGE_MAX * 2 + 8];   // the text with its line breaks
static size_t wrapped_len;
static size_t typed;            // how much of it is on screen
static int scale;
static int cols, rows;          // how many characters fit across and down at this scale
static uint32_t shown_revision = UINT32_MAX;
static int shown_size = -1;
static uint32_t blink_ms;
static bool cursor_on;

/* Break text into lines of at most cols characters, at spaces where it can.
 * Returns the number of lines, and the length of the longest in *widest. */
static int wrap(const char *text, int cols, char *out, size_t out_size, size_t *out_len, int *widest)
{
    size_t n = 0;
    int lines = 1, col = 0;
    *widest = 0;
    const char *p = text;
    while (*p != '\0' && n + 2 < out_size)
    {
        if (*p == '\n')
        {
            out[n++] = '\n';
            lines++;
            col = 0;
            p++;
            continue;
        }
        if (*p == ' ')
        {
            // A space between words: kept if the line has room, dropped at a break
            if (col > 0 && col < cols)
            {
                out[n++] = ' ';
                col++;
            }
            p++;
            continue;
        }
        size_t word = strcspn(p, " \n");
        if (col > 0 && col + (int)word > cols)
        {
            // Doesn't fit after what's on the line: start a new one (dropping the
            // space before it)
            if (out[n - 1] == ' ')
            {
                n--;
            }
            out[n++] = '\n';
            lines++;
            col = 0;
        }
        while (word > 0 && n + 2 < out_size)
        {
            if (col == cols)
            {
                out[n++] = '\n';    // a word longer than a line: break it
                lines++;
                col = 0;
            }
            out[n++] = *p++;
            word--;
            col++;
            if (col > *widest)
            {
                *widest = col;
            }
        }
    }
    *out_len = n;
    out[n] = '\0';
    return lines;
}

/* Where the cursor goes after the first `count` characters. */
static void cursor_cell(size_t count, int cols, int *col, int *row)
{
    *col = 0;
    *row = 0;
    for (size_t i = 0; i < count; i++)
    {
        if (wrapped[i] == '\n')
        {
            (*row)++;
            *col = 0;
        }
        else
        {
            (*col)++;
        }
    }
    if (*col >= cols)
    {
        (*row)++;   // the line's full: it goes at the start of the next
        *col = 0;
    }
}

/* Lay out the message at the size chosen, or the biggest it fits at, and
 * size the box: just big enough for it if it fits, otherwise the whole
 * square, with the text scrolling up as it types. */
static void layout(void)
{
    const char *text = message_text();
    if (text[0] == '\0')
    {
        text = PROMPT;
    }
    int32_t side = ui_px(BOX_DESIGN_PX);
    char old[sizeof(wrapped)];
    memcpy(old, wrapped, typed);
    int old_scale = scale;

    int size = settings_get()->terminal_size;
    int widest = 0, end_col = 0, end_row = 0;
    for (scale = size > 0 ? LV_MIN(size, MAX_SCALE) : MAX_SCALE; scale >= 1; scale--)
    {
        cols = side / (8 * scale);
        rows = side / (9 * scale);
        wrap(text, cols, wrapped, sizeof(wrapped), &wrapped_len, &widest);
        cursor_cell(wrapped_len, cols, &end_col, &end_row);
        if (size > 0 || end_row + 1 <= rows || scale == AUTO_MIN_SCALE)
        {
            break;      // the size chosen, or the biggest that fits
        }
    }
    bool fits = end_row + 1 <= rows;
    int width = fits ? LV_MAX(widest, end_col + 1) : cols;
    int height = fits ? end_row + 1 : rows;

    // Carry on typing if the new text only adds to what's on screen
    if (scale != old_scale || typed > wrapped_len || memcmp(old, wrapped, typed) != 0)
    {
        typed = 0;
    }

    lv_obj_set_style_text_font(label, fonts[scale], 0);
    lv_obj_set_size(box, width * 8 * scale, height * 9 * scale);
    lv_obj_center(box);
    lv_obj_set_size(cursor, 7 * scale, 7 * scale);
    lv_obj_set_style_shadow_width(cursor, 3 * scale, 0);
    message_set_scale(scale);
}

/* What's typed so far: its last `rows` lines, if there are more */
static void show_typed(void)
{
    int col, row;
    cursor_cell(typed, cols, &col, &row);
    int first_row = LV_MAX(0, row + 1 - rows);
    size_t from = 0;
    for (int skipped = 0; skipped < first_row && from < typed; from++)
    {
        if (wrapped[from] == '\n')
        {
            skipped++;
        }
    }
    char shown[sizeof(wrapped)];
    memcpy(shown, wrapped + from, typed - from);
    shown[typed - from] = '\0';
    lv_label_set_text(label, shown);
    lv_obj_set_pos(cursor, col * 8 * scale, (row - first_row) * 9 * scale + scale);
}

static void tick_cb(lv_timer_t *t)
{
    if (message_revision() != shown_revision)
    {
        shown_revision = message_revision();
        layout();
        show_typed();
    }
    if (settings_get()->terminal_size != shown_size)
    {
        // A new size: show it all at once, rather than typing it out again
        shown_size = settings_get()->terminal_size;
        layout();
        typed = wrapped_len;
        show_typed();
    }
    if (ui_current_page() != page)
    {
        return;     // types out only while it's on screen
    }

    uint32_t now = lv_tick_get();
    if (typed < wrapped_len)
    {
        // Faster for long messages, so they don't take all day
        size_t step = 1 + (wrapped_len - typed) / 120;
        typed = LV_MIN(typed + step, wrapped_len);
        show_typed();
        cursor_on = true;       // solid while it types
        blink_ms = now;
        lv_obj_set_hidden(cursor, false);
    }
    else if (now - blink_ms >= BLINK_MS)
    {
        blink_ms = now;
        cursor_on = !cursor_on;
        lv_obj_set_hidden(cursor, !cursor_on);
    }
}

/* A strip as wide as the screen with a dark line at the bottom, tiled down
 * the page for the look of a CRT's scan lines. */
static void add_scanlines(void)
{
    int32_t w = lv_display_get_horizontal_resolution(lv_display_get_default());
    uint32_t stride = (uint32_t)w * 4;
    uint8_t *pixels = lv_malloc(stride * SCANLINE_PERIOD);
    if (pixels == NULL)
    {
        return;
    }
    memset(pixels, 0, stride * SCANLINE_PERIOD);
    for (int32_t x = 0; x < w; x++)
    {
        pixels[stride * (SCANLINE_PERIOD - 1) + x * 4 + 3] = SCANLINE_OPA;     // black, partly see-through
    }
    scanlines.header.magic = LV_IMAGE_HEADER_MAGIC;
    scanlines.header.cf = LV_COLOR_FORMAT_ARGB8888;
    scanlines.header.w = (uint32_t)w;
    scanlines.header.h = SCANLINE_PERIOD;
    scanlines.header.stride = stride;
    scanlines.data = pixels;
    scanlines.data_size = stride * SCANLINE_PERIOD;

    lv_obj_t *img = lv_image_create(page);
    lv_image_set_src(img, &scanlines);
    lv_obj_set_size(img, LV_PCT(100), LV_PCT(100));
    lv_image_set_inner_align(img, LV_IMAGE_ALIGN_TILE);
    lv_obj_set_clickable(img, false);
}

static void on_navigate(void)
{
    if (ui_current_page() == page)
    {
        blink_ms = lv_tick_get();
    }
}

static bool build(ui_feature_t *feature)
{
    page = ui_feature_add_page(feature, "Terminal", true);
    lv_obj_set_scrollable(page, false);

    box = lv_obj_create(page);
    lv_obj_remove_style_all(box);
    lv_obj_set_scrollable(box, false);
    lv_obj_set_clickable(box, false);
    lv_obj_set_overflow_visible(box, true);     // for the cursor's glow

    label = lv_label_create(box);
    lv_obj_set_style_text_color(label, COLOR_PHOSPHOR, 0);
    lv_obj_set_style_text_line_space(label, 0, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_pos(label, 0, 0);
    lv_obj_set_size(label, LV_PCT(100), LV_PCT(100));

    cursor = lv_obj_create(box);
    lv_obj_remove_style_all(cursor);
    lv_obj_set_style_bg_color(cursor, COLOR_PHOSPHOR, 0);
    lv_obj_set_style_bg_opa(cursor, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_color(cursor, COLOR_PHOSPHOR, 0);
    lv_obj_set_style_shadow_opa(cursor, LV_OPA_50, 0);
    lv_obj_set_clickable(cursor, false);

    add_scanlines();

    ui_add_startup_choice(feature, page, "Terminal", "terminal");
    lv_timer_create(tick_cb, TICK_MS, NULL);
    tick_cb(NULL);
    return true;
}
