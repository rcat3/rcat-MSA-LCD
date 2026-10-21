/*
 * Plays sound files from storage through the board's speaker, in the
 * background, while the display carries on as normal.
 *
 * The playlist is every WAV, MP3 and FLAC file in the "sounds" folder,
 * sorted by name. Files are read and decoded as they play, on the board's
 * speaker task, so they can be any length. When a track ends, the player
 * moves on to the next one, plays the same one again, or stops (see
 * audio_repeat_t). A track that's played again starts straight after its
 * own end, so a sound effect can loop without a gap. MP3s loop without a
 * gap too if the encoder recorded how much silence it added to each end
 * (LAME and ffmpeg do); otherwise there's a short silence at the loop.
 *
 * The UI and the speaker task share very little: the UI posts a command
 * (play, pause or resume) and the speaker task carries it out the next time
 * it asks for sound. The speaker task publishes which track is playing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio/audio_player.h"
#include "audio/speaker.h"

#if RCAT_HAVE_FILESYSTEM

#include <dirent.h>
#include <strings.h>

#include "audio/audio_decoders.h"

#define SOUNDS_FOLDER       "sounds"
#define MAX_TRACKS          100
#define MAX_FOLDER_LEN      64
#define MAX_PATH_LEN        320

// Files are decoded this many frames at a time, at most.
#define SCRATCH_FRAMES      256
#define MAX_CHANNELS        8

typedef enum
{
    CMD_PLAY,
    CMD_CUE,
    CMD_PAUSE,
    CMD_RESUME,
} command_t;

typedef enum
{
    DECODER_NONE,
    DECODER_WAV,
    DECODER_MP3,
    DECODER_FLAC,
} decoder_kind_t;

typedef struct
{
    decoder_kind_t kind;
    uint32_t sample_rate;
    uint32_t channels;
    union
    {
        drwav wav;
        drmp3 mp3;
        drflac *flac;
    } u;
} decoder_t;

static bool available;
static char folder[MAX_FOLDER_LEN];
static char *file_names[MAX_TRACKS];
static char *track_names[MAX_TRACKS];
static size_t track_count;

// Written by the UI, read by the speaker task. The command and its track are
// written first, then cmd_seq is bumped to post it.
static volatile uint8_t cmd;
static volatile uint16_t cmd_track;
static uint32_t cmd_seq;
static volatile uint8_t volume = 50;
static volatile uint8_t repeat = AUDIO_REPEAT_ALL;

// Written by the speaker task (and by the UI when it posts a command, so it
// sees the change straight away), read by the UI.
static volatile bool playing;
static volatile uint16_t current;

// Speaker task state
static uint32_t seen_seq;
static bool active;             // playing, rather than paused or stopped
static uint16_t track;
static decoder_t *decoder;      // too big for the stack, and for internal RAM
static int16_t *scratch;
static uint32_t frames_this_track;

/* ----- Decoding ----- */

static bool has_extension(const char *file_name, const char *ext)
{
    size_t len = strlen(file_name);
    size_t ext_len = strlen(ext);
    return len > ext_len && strcasecmp(file_name + len - ext_len, ext) == 0;
}

static bool is_sound_file(const char *file_name)
{
    // Skip hidden files, including the "._name" files macOS leaves on cards.
    return file_name[0] != '.'
        && (has_extension(file_name, ".wav")
            || has_extension(file_name, ".mp3")
            || has_extension(file_name, ".flac"));
}

static void decoder_close(void)
{
    switch (decoder->kind)
    {
        case DECODER_WAV:
            drwav_uninit(&decoder->u.wav);
            break;
        case DECODER_MP3:
            drmp3_uninit(&decoder->u.mp3);
            break;
        case DECODER_FLAC:
            drflac_close(decoder->u.flac);
            break;
        case DECODER_NONE:
            break;
    }
    decoder->kind = DECODER_NONE;
}

static bool decoder_open(size_t index)
{
    char path[MAX_PATH_LEN];
    const char *name = file_names[index];
    if (snprintf(path, sizeof(path), "%s/%s", folder, name) >= (int)sizeof(path))
    {
        return false;
    }

    if (has_extension(name, ".wav") && drwav_init_file(&decoder->u.wav, path, NULL))
    {
        decoder->kind = DECODER_WAV;
        decoder->sample_rate = decoder->u.wav.sampleRate;
        decoder->channels = decoder->u.wav.channels;
    }
    else if (has_extension(name, ".mp3") && drmp3_init_file(&decoder->u.mp3, path, NULL))
    {
        decoder->kind = DECODER_MP3;
        decoder->sample_rate = decoder->u.mp3.sampleRate;
        decoder->channels = decoder->u.mp3.channels;
    }
    else if (has_extension(name, ".flac") && (decoder->u.flac = drflac_open_file(path, NULL)) != NULL)
    {
        decoder->kind = DECODER_FLAC;
        decoder->sample_rate = decoder->u.flac->sampleRate;
        decoder->channels = decoder->u.flac->channels;
    }
    else
    {
        printf("sound: can't play %s\n", name);
        return false;
    }

    if (decoder->sample_rate == 0 || decoder->channels == 0 || decoder->channels > MAX_CHANNELS)
    {
        printf("sound: can't play %s (%lu channels at %lu Hz)\n", name,
               (unsigned long)decoder->channels, (unsigned long)decoder->sample_rate);
        decoder_close();
        return false;
    }
    printf("sound: playing %s\n", name);
    return true;
}

/* Decode up to max_frames frames as stereo. Returns 0 at the end. */
static size_t decoder_read(int16_t *out, size_t max_frames)
{
    size_t frames = (max_frames < SCRATCH_FRAMES) ? max_frames : SCRATCH_FRAMES;
    uint32_t channels = decoder->channels;
    int16_t *dest = (channels == 2) ? out : scratch;
    size_t got = 0;

    switch (decoder->kind)
    {
        case DECODER_WAV:
            got = drwav_read_pcm_frames_s16(&decoder->u.wav, frames, dest);
            break;
        case DECODER_MP3:
            got = drmp3_read_pcm_frames_s16(&decoder->u.mp3, frames, dest);
            break;
        case DECODER_FLAC:
            got = drflac_read_pcm_frames_s16(decoder->u.flac, frames, dest);
            break;
        case DECODER_NONE:
            break;
    }

    // Mono plays on both sides. With more than two channels, the first two
    // are the front left and right.
    if (channels == 1)
    {
        for (size_t i = 0; i < got; i++)
        {
            out[2 * i] = out[2 * i + 1] = scratch[i];
        }
    }
    else if (channels > 2)
    {
        for (size_t i = 0; i < got; i++)
        {
            out[2 * i] = scratch[channels * i];
            out[2 * i + 1] = scratch[channels * i + 1];
        }
    }
    return got;
}

/* ----- Speaker task ----- */

static void publish(void)
{
    current = track;
    playing = active;
}

static void stop(void)
{
    decoder_close();
    active = false;
    publish();
}

static void handle_commands(void)
{
    uint32_t seq = __atomic_load_n(&cmd_seq, __ATOMIC_ACQUIRE);
    if (seq == seen_seq)
    {
        return;
    }
    seen_seq = seq;

    switch (cmd)
    {
        case CMD_PLAY:
            decoder_close();
            track = (cmd_track < track_count) ? cmd_track : 0;
            active = true;
            break;
        case CMD_CUE:
            decoder_close();
            track = (cmd_track < track_count) ? cmd_track : 0;
            active = false;
            break;
        case CMD_PAUSE:
            active = false;
            break;
        case CMD_RESUME:
            active = true;     // from where it was paused, or the start if it had stopped
            break;
    }
    publish();
}

/* Volume in percent to a gain out of 65536. Loudness goes roughly with the
 * square of the gain, so the slider feels even. */
static int32_t volume_gain(void)
{
    int32_t v = volume;
    return v * v * 65536 / (100 * 100);
}

size_t audio_player_fill(int16_t *frames, size_t max_frames, uint32_t *sample_rate)
{
    if (decoder == NULL)
    {
        return 0;
    }
    handle_commands();

    size_t done = 0;
    uint32_t block_rate = 0;
    size_t failures = 0;

    while (active && done < max_frames)
    {
        if (decoder->kind == DECODER_NONE)
        {
            if (!decoder_open(track))
            {
                // Skip files that can't be played, unless they all fail.
                if (repeat != AUDIO_REPEAT_ALL || ++failures >= track_count)
                {
                    stop();
                    break;
                }
                track = (track + 1) % track_count;
                publish();
                continue;
            }
            frames_this_track = 0;
        }

        // Each block is at one sample rate. A track at a different rate
        // starts in the next block.
        if (block_rate == 0)
        {
            block_rate = decoder->sample_rate;
        }
        else if (decoder->sample_rate != block_rate)
        {
            break;
        }

        size_t got = decoder_read(frames + 2 * done, max_frames - done);
        if (got > 0)
        {
            done += got;
            frames_this_track += got;
            failures = 0;
            continue;
        }

        // The end of the track. Stop if tracks keep ending without playing
        // anything, rather than opening them over and over.
        decoder_close();
        if (frames_this_track == 0 && ++failures > track_count)
        {
            stop();
            break;
        }
        if (repeat == AUDIO_REPEAT_OFF)
        {
            stop();
            break;
        }
        if (repeat == AUDIO_REPEAT_ALL)
        {
            track = (track + 1) % track_count;
        }
        publish();
    }

    if (done > 0)
    {
        int32_t gain = volume_gain();
        for (size_t i = 0; i < 2 * done; i++)
        {
            frames[i] = (int16_t)((frames[i] * gain) >> 16);
        }
        *sample_rate = block_rate;
    }
    return done;
}

/* ----- Public ----- */

static int compare_names(const void *a, const void *b)
{
    return strcasecmp(*(const char * const *)a, *(const char * const *)b);
}

static void find_tracks(const char *storage_path)
{
    if (snprintf(folder, sizeof(folder), "%s/" SOUNDS_FOLDER, storage_path) >= (int)sizeof(folder))
    {
        return;
    }
    DIR *dir = opendir(folder);
    if (dir == NULL)
    {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && track_count < MAX_TRACKS)
    {
        if (entry->d_type != DT_DIR && is_sound_file(entry->d_name))
        {
            file_names[track_count++] = strdup(entry->d_name);
        }
    }
    closedir(dir);
    qsort(file_names, track_count, sizeof(file_names[0]), compare_names);

    for (size_t i = 0; i < track_count; i++)
    {
        track_names[i] = strdup(file_names[i]);
        *strrchr(track_names[i], '.') = '\0';
        printf("sound: found %s\n", file_names[i]);
    }
}

bool audio_player_init(const char *storage_path)
{
    if (storage_path != NULL)
    {
        find_tracks(storage_path);
    }

    decoder = calloc(1, sizeof(*decoder));
    scratch = malloc(SCRATCH_FRAMES * MAX_CHANNELS * sizeof(int16_t));
    if (decoder == NULL || scratch == NULL)
    {
        printf("sound: not enough memory\n");
        return false;
    }

    available = speaker_start();
    return available;
}

size_t audio_player_track_count(void)
{
    return track_count;
}

const char *audio_player_track_name(size_t index)
{
    return (index < track_count) ? track_names[index] : "";
}

static void post(command_t command, size_t index)
{
    cmd = command;
    cmd_track = (uint16_t)index;
    __atomic_add_fetch(&cmd_seq, 1, __ATOMIC_RELEASE);
}

void audio_player_play(size_t index)
{
    if (track_count == 0)
    {
        return;
    }
    current = (uint16_t)(index % track_count);
    playing = true;
    post(CMD_PLAY, current);
}

void audio_player_cue(size_t index)
{
    if (track_count == 0)
    {
        return;
    }
    current = (uint16_t)(index % track_count);
    playing = false;
    post(CMD_CUE, current);
}

void audio_player_pause(void)
{
    playing = false;
    post(CMD_PAUSE, 0);
}

void audio_player_resume(void)
{
    if (track_count == 0)
    {
        return;
    }
    playing = true;
    post(CMD_RESUME, 0);
}

void audio_player_set_volume(uint8_t percent)
{
    volume = (percent > 100) ? 100 : percent;
}

void audio_player_set_repeat(audio_repeat_t r)
{
    repeat = (r < AUDIO_REPEAT_COUNT) ? r : AUDIO_REPEAT_ALL;
}

#else   // no filesystem, so nothing to play

bool audio_player_init(const char *storage_path)
{
    return false;
}

size_t audio_player_track_count(void)
{
    return 0;
}

const char *audio_player_track_name(size_t index)
{
    return "";
}

size_t audio_player_fill(int16_t *frames, size_t max_frames, uint32_t *sample_rate)
{
    return 0;
}

void audio_player_play(size_t index) {}
void audio_player_cue(size_t index) {}
void audio_player_pause(void) {}
void audio_player_resume(void) {}
void audio_player_set_volume(uint8_t percent) {}
void audio_player_set_repeat(audio_repeat_t r) {}

static const bool available = false;
static const bool playing = false;
static const uint16_t current = 0;

#endif

bool audio_player_available(void)
{
    return available;
}

bool audio_player_is_playing(void)
{
    return playing;
}

size_t audio_player_current(void)
{
    return current;
}
