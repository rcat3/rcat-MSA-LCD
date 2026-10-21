#ifndef RCAT_SPEAKER_H
#define RCAT_SPEAKER_H

#include <stdbool.h>

/* The speaker, shared by the sound player and the voice effect. While the
 * voice effect is switched to the speaker, it has the speaker and the player
 * waits. Returns false if the board has no speaker. Safe to call more than
 * once. */
bool speaker_start(void);

#endif
