/*
 * Microphone level meter with an automatically adjusting range.
 *
 * Each block of samples from the board's audio task goes through a DC
 * blocking filter (MEMS microphones have an offset), then its RMS level is
 * converted to dB. Two trackers then set the range the meter shows:
 *
 * - The background level is the quietest 100 ms in the last few seconds.
 *   The gaps between words keep it low while you talk, and in a noisy place
 *   it catches up within BACKGROUND_WINDOW_S. The bottom of the meter sits
 *   GATE_ABOVE_BACKGROUND_DB above it, so the surroundings don't light the
 *   bars, whether that's a quiet room or a noisy one.
 * - The speech level follows the loudest recent sounds, jumping up quickly
 *   and relaxing slowly. The top of the meter sits just below it, so normal
 *   talking uses most of the meter, and after loud talking the range scales
 *   back and then recovers over a few seconds.
 *
 * The range never shrinks below MIN_RANGE_DB, so the meter doesn't turn
 * background noise into a full display. The result rises straight away and
 * falls back at LEVEL_FALL_PER_SECOND.
 *
 * This is the first stage of the audio path. Voice effects will process the
 * same blocks later.
 */
#include <math.h>
#include <stdio.h>

#include "audio/audio_level.h"
#include "hal.h"

#define SAMPLE_RATE                 16000

// Background tracking: the quietest 100 ms slot in the last 3 seconds. Each
// slot is an average, because individual blocks in a quiet room swing by
// around 10 dB.
#define BACKGROUND_SLOT_MS          100
#define BACKGROUND_WINDOW_S         3
#define BACKGROUND_SLOTS            (BACKGROUND_WINDOW_S * 1000 / BACKGROUND_SLOT_MS)
#define GATE_ABOVE_BACKGROUND_DB    12.0f   // bottom of the meter, above the background

// Speech tracking
#define SPEECH_ATTACK_TIME_S        0.1f    // how quickly it follows louder speech
#define SPEECH_RELEASE_DB_PER_S     6.0f    // how quickly it relaxes afterwards
#define SPEECH_HEADROOM_DB          3.0f    // the meter is full this far below the speech level

// The smallest range the meter can show, in dB from bottom to top.
#define MIN_RANGE_DB                15.0f

// How fast the meter falls back, in full scales per second.
#define LEVEL_FALL_PER_SECOND       6.0f

// DC blocking filter pole. Closer to 1 keeps more of the low frequencies.
#define DC_BLOCK_POLE               0.995f

// Set to 1 to print the measured levels once a second, for tuning the above.
#define AUDIO_LEVEL_DEBUG           0

static bool running;
static volatile uint16_t level;     // written by the audio task, read by the UI

// Audio task state
static bool have_levels;
static float slot_db[BACKGROUND_SLOTS];     // average level of each finished slot
static uint8_t slot_index;
static uint32_t slot_samples;
static float slot_sum_squares;
static float background_db;
static float speech_db;
static float envelope;
static float dc_prev_in;
static float dc_prev_out;

#if AUDIO_LEVEL_DEBUG
static float debug_loudest_db = -120.0f;
static uint32_t debug_samples;
#endif

static float clamp01(float x)
{
    return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

static float to_db(float mean_square)
{
    return 10.0f * log10f(mean_square / (32768.0f * 32768.0f) + 1e-18f);
}

static float block_mean_square(const int16_t *samples, size_t count)
{
    float sum_squares = 0.0f;
    for (size_t i = 0; i < count; i++)
    {
        float in = samples[i];
        float out = in - dc_prev_in + DC_BLOCK_POLE * dc_prev_out;
        dc_prev_in = in;
        dc_prev_out = out;
        sum_squares += out * out;
    }
    return sum_squares / count;
}

static void block_cb(const int16_t *samples, size_t count, void *user_data)
{
    if (count == 0)
    {
        return;
    }

    float mean_square = block_mean_square(samples, count);
    float db = to_db(mean_square);
    float dt = (float)count / SAMPLE_RATE;

    if (!have_levels)
    {
        for (int i = 0; i < BACKGROUND_SLOTS; i++)
        {
            slot_db[i] = db;
        }
        speech_db = db + GATE_ABOVE_BACKGROUND_DB + MIN_RANGE_DB + SPEECH_HEADROOM_DB;
        have_levels = true;
    }

    // Background: the quietest finished slot in the window.
    slot_sum_squares += mean_square * count;
    slot_samples += count;
    if (slot_samples >= SAMPLE_RATE * BACKGROUND_SLOT_MS / 1000)
    {
        slot_db[slot_index] = to_db(slot_sum_squares / slot_samples);
        slot_index = (slot_index + 1) % BACKGROUND_SLOTS;
        slot_samples = 0;
        slot_sum_squares = 0.0f;
    }
    background_db = slot_db[0];
    for (int i = 1; i < BACKGROUND_SLOTS; i++)
    {
        background_db = fminf(background_db, slot_db[i]);
    }
    float bottom_db = background_db + GATE_ABOVE_BACKGROUND_DB;

    // Speech: up quickly, down slowly, but always leaving at least MIN_RANGE_DB.
    if (db > speech_db)
    {
        speech_db += (db - speech_db) * (1.0f - expf(-dt / SPEECH_ATTACK_TIME_S));
    }
    else
    {
        speech_db -= SPEECH_RELEASE_DB_PER_S * dt;
    }
    speech_db = fmaxf(speech_db, bottom_db + MIN_RANGE_DB + SPEECH_HEADROOM_DB);
    float top_db = speech_db - SPEECH_HEADROOM_DB;

    float target = clamp01((db - bottom_db) / (top_db - bottom_db));

    if (target > envelope)
    {
        envelope = target;
    }
    else
    {
        envelope = fmaxf(target, envelope - LEVEL_FALL_PER_SECOND * dt);
    }
    level = (uint16_t)(envelope * AUDIO_LEVEL_MAX);

#if AUDIO_LEVEL_DEBUG
    debug_loudest_db = fmaxf(debug_loudest_db, db);
    debug_samples += count;
    if (debug_samples >= SAMPLE_RATE)
    {
        printf("audio: now %.1f dB, loudest %.1f dB, meter range %.1f to %.1f dB (background %.1f dB)\n",
               db, debug_loudest_db, bottom_db, top_db, background_db);
        debug_samples = 0;
        debug_loudest_db = -120.0f;
    }
#endif
}

bool audio_level_start(void)
{
    if (!running)
    {
        running = rcat_hal_audio_in_start(SAMPLE_RATE, block_cb, NULL);
    }
    return running;
}

bool audio_level_available(void)
{
    return running;
}

uint16_t audio_level_get(void)
{
    return level;
}
