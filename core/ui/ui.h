#ifndef RCAT_UI_H
#define RCAT_UI_H

#include <stddef.h>

/* Build the tile view: one tile per image, a black tile, then the settings tiles. */
void ui_init(void);

/*
 * Moving around from outside the UI (the phone remote). Features are counted
 * top to bottom, and the pages in each one left to right, as on screen.
 * Only call these from the UI loop.
 */
size_t ui_feature_count(void);
const char *ui_feature_name(size_t feature);
size_t ui_page_count(size_t feature);
const char *ui_page_name(size_t feature, size_t page);

/* Which feature and page are on screen. */
void ui_current(size_t *feature, size_t *page);

/* Bring a page on screen, as if it had been swiped to. */
void ui_show(size_t feature, size_t page);

#endif
