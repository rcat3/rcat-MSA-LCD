#ifndef RCAT_AUDIO_PLAYER_H
#define RCAT_AUDIO_PLAYER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* What happens at the end of a track. */
typedef enum
{
    AUDIO_REPEAT_ALL,       // play the next track, and start again after the last one
    AUDIO_REPEAT_ONE,       // play the same track again
    AUDIO_REPEAT_OFF,       // stop
    AUDIO_REPEAT_COUNT
} audio_repeat_t;

/* Find the sound files in a folder on storage (its "sounds" subfolder) and
 * start the speaker. Returns false if the board has no speaker. Finding no
 * files isn't an error: the player is still there, with nothing to play. */
bool audio_player_init(const char *storage_path);

/* True once audio_player_init() has succeeded. */
bool audio_player_available(void);

/* The playlist: every sound file found, sorted by name. Names are shown
 * without the file extension. */
size_t audio_player_track_count(void);
const char *audio_player_track_name(size_t index);

/* Play a track from the start. */
void audio_player_play(size_t index);

/* Choose a track without playing it. Resuming plays it from the start. */
void audio_player_cue(size_t index);

/* Pause and carry on from the same place. */
void audio_player_pause(void);
void audio_player_resume(void);

bool audio_player_is_playing(void);

/* The track that's playing or paused, or was played last. */
size_t audio_player_current(void);

void audio_player_set_volume(uint8_t percent);
void audio_player_set_repeat(audio_repeat_t repeat);

/* For the speaker: decode up to max_frames frames of stereo (see
 * rcat_hal_audio_fill_cb_t). Called on the speaker's task. */
size_t audio_player_fill(int16_t *frames, size_t max_frames, uint32_t *sample_rate);

#endif
