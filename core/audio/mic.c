/*
 * Hands each block of microphone samples to every listener.
 */
#include "audio/mic.h"
#include "hal.h"

#define MAX_LISTENERS 4

typedef struct
{
    mic_listener_t cb;
    void *user_data;
} listener_t;

static listener_t listeners[MAX_LISTENERS];
static volatile int listener_count;
static bool running;

static void block_cb(const int16_t *samples, size_t count, void *user_data)
{
    int n = __atomic_load_n(&listener_count, __ATOMIC_ACQUIRE);
    for (int i = 0; i < n; i++)
    {
        listeners[i].cb(samples, count, listeners[i].user_data);
    }
}

bool mic_start(void)
{
    if (!running)
    {
        running = rcat_hal_audio_in_start(MIC_SAMPLE_RATE, block_cb, NULL);
    }
    return running;
}

bool mic_add_listener(mic_listener_t cb, void *user_data)
{
    int n = listener_count;
    if (n >= MAX_LISTENERS)
    {
        return false;
    }
    // Fill in the listener before it's counted, so the audio task never
    // sees a half written one.
    listeners[n].cb = cb;
    listeners[n].user_data = user_data;
    __atomic_store_n(&listener_count, n + 1, __ATOMIC_RELEASE);
    return true;
}
