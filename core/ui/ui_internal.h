#ifndef RCAT_UI_INTERNAL_H
#define RCAT_UI_INTERNAL_H

/*
 * The UI is a stack of features that you move between by swiping up and
 * down. Each feature has one or more pages side by side that you move
 * between by swiping left and right: the images feature has a page per
 * image, the settings feature has a page per setting, and so on.
 *
 * A feature is a ui_feature_def_t (see the feature_*.c files) listed in
 * ui.c. Its build function adds its pages with ui_feature_add_page().
 */

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

typedef struct ui_feature ui_feature_t;

typedef struct
{
    const char *name;

    /* Add the feature's pages. Return false to leave the feature out, for
     * example when the board doesn't have the hardware it needs. */
    bool (*build)(ui_feature_t *feature);

    /* Optional. Called whenever a different page comes on screen. */
    void (*on_navigate)(void);
} ui_feature_def_t;

/* Scale a size or offset from the 240x240 design to this screen. */
int32_t ui_px(int32_t design_px);

/* Add a page to the end of a feature. black gives it a black background, for
 * pages that show images or effects rather than controls. */
lv_obj_t *ui_feature_add_page(ui_feature_t *feature, bool black);

/* The feature and page currently on screen. */
bool ui_feature_is_current(const ui_feature_t *feature);
lv_obj_t *ui_current_page(void);

/*
 * Startup choices: pages that can be picked in the Startup setting. value is
 * what's saved (see settings_set_startup_image) and label is what's shown.
 * Both must stay valid for as long as the UI exists.
 */
void ui_add_startup_choice(ui_feature_t *feature, lv_obj_t *page, const char *label, const char *value);
size_t ui_startup_choice_count(void);
const char *ui_startup_choice_label(size_t index);
const char *ui_startup_choice_value(size_t index);

/* Index of the startup choice with this value, or -1. */
int ui_find_startup_choice(const char *value);

/* True while the images feature is on screen (the voice ring shows over it). */
bool feature_images_on_screen(void);

#endif
