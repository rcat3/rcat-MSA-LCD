#ifndef RCAT_MIC_H
#define RCAT_MIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The microphone, shared by everything that listens to it (the level meter,
 * the voice lab). Samples are mono, signed 16-bit, at MIC_SAMPLE_RATE. */
#define MIC_SAMPLE_RATE 32000

/* Called with each block of samples (up to 10 ms), on the board's audio task, not
 * the UI loop. It has to be quick and must not call LVGL. */
typedef void (*mic_listener_t)(const int16_t *samples, size_t count, void *user_data);

/* Start the microphone. Returns false if the board doesn't have one. Safe to
 * call more than once. */
bool mic_start(void);

/* Add a listener. Call it before or after mic_start(), from the UI loop.
 * Returns false if there's no room for another one. */
bool mic_add_listener(mic_listener_t cb, void *user_data);

#endif
