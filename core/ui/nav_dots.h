#ifndef RCAT_NAV_DOTS_H
#define RCAT_NAV_DOTS_H

#include <stdint.h>

/* Page and feature position dots, drawn on LVGL's top layer. */
void nav_dots_create(void);

/* Show where we are, briefly. Indexes start at 0. */
void nav_dots_show(uint8_t feature, uint8_t feature_count, uint8_t page, uint8_t page_count);

#endif
