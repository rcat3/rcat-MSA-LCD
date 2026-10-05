#ifndef RCAT_AUDIO_DECODERS_H
#define RCAT_AUDIO_DECODERS_H

/*
 * The sound file decoders, from dr_libs (third_party/dr_libs). They're built
 * once, in audio_decoders.c, with the options set here.
 */

#define DR_WAV_NO_WCHAR     // no wide character file names
#define DR_FLAC_NO_OGG      // FLAC in .flac files only, not inside Ogg

#include "dr_flac.h"
#include "dr_mp3.h"
#include "dr_wav.h"

#endif
