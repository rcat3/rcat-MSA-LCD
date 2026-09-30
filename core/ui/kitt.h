#ifndef RCAT_KITT_H
#define RCAT_KITT_H

#include "lvgl.h"

/* Build a KITT style voice box (three columns of red bars that follow the
 * microphone level) centred in parent. Needs audio_level_start() first. */
void kitt_create(lv_obj_t *parent);

#endif
