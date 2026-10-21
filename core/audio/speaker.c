/*
 * Hands the speaker to the voice effect or the sound player.
 */
#include "audio/audio_player.h"
#include "audio/speaker.h"
#include "audio/voice_fx.h"
#include "hal.h"

static size_t fill_cb(int16_t *frames, size_t max_frames, uint32_t *sample_rate, void *user_data)
{
    if (voice_fx_speaker_on())
    {
        return voice_fx_speaker_fill(frames, max_frames, sample_rate);
    }
    return audio_player_fill(frames, max_frames, sample_rate);
}

bool speaker_start(void)
{
    static bool started;
    static bool tried;
    if (!tried)
    {
        tried = true;
        started = rcat_hal_audio_out_start(fill_cb, NULL);
    }
    return started;
}
