/*
 * Builds the dr_libs decoders used by the audio player. They need a
 * filesystem to read from, so they're left out on platforms without one.
 */
#if RCAT_HAVE_FILESYSTEM

#define DR_FLAC_IMPLEMENTATION
#define DR_MP3_IMPLEMENTATION
#define DR_WAV_IMPLEMENTATION
#include "audio/audio_decoders.h"

#endif
