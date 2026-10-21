#ifndef RCAT_VOICE_FX_H
#define RCAT_VOICE_FX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Voice effects, run on the microphone as it comes in. They're ports of the
 * effects in tools/voice_monitor.py, with the same presets, and run at
 * VOICE_FX_SAMPLE_RATE, which is plenty for speech.
 *
 * The result goes to the voice lab stream (so voice_monitor.py --listen
 * effect can play it) and, if switched on, to the speaker.
 */
#define VOICE_FX_SAMPLE_RATE 16000

/* Start listening to the microphone. Returns false without one. */
bool voice_fx_start(void);

/* The presets, with 0 being "Off". */
size_t voice_fx_preset_count(void);
const char *voice_fx_preset_name(size_t index);

/* Choose a preset (0 for off). Safe to call from the UI loop. */
void voice_fx_set_preset(size_t index);

/* Play the processed voice on the speaker instead of the sound player. */
void voice_fx_set_speaker(bool on);
bool voice_fx_speaker_on(void);

/* For the speaker: up to max_frames frames of the processed voice as
 * stereo, at VOICE_FX_SAMPLE_RATE. Called on the speaker's task. */
size_t voice_fx_speaker_fill(int16_t *frames, size_t max_frames, uint32_t *sample_rate);

#endif
