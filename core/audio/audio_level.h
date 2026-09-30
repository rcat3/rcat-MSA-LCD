#ifndef RCAT_AUDIO_LEVEL_H
#define RCAT_AUDIO_LEVEL_H

#include <stdbool.h>
#include <stdint.h>

#define AUDIO_LEVEL_MAX 1000

/* Start measuring the microphone level. Returns false if the board has no
 * microphone. */
bool audio_level_start(void);

/* True once audio_level_start() has succeeded. */
bool audio_level_available(void);

/* The current level, 0 (quiet) to AUDIO_LEVEL_MAX (loud). It jumps up with
 * the sound and falls back smoothly, like a VU meter. Safe to call from the
 * UI loop. */
uint16_t audio_level_get(void);

#endif
