# Sound

*Boards with a speaker: the ESP32-S3 boards*

The Sound feature plays sound files from the SD card.  Its pages are:

- **Player**: the track that's playing, with previous, play/pause and next buttons.  Sound keeps playing in the background while you look at images or anything else.
- **Options**: the volume, what happens at the end of a track (*Repeat all*, *Repeat one* or *Play once*), and *Play at startup*, which starts playing the last track as soon as the display turns on.

On the ESP32-S3-Touch-LCD-1.46, the small round trimmer on the back of the board also sets the built-in speaker's volume, on top of the volume setting.  Which speaker the sound goes to is chosen when the firmware is built (see [the 1.46" board's build guide](../hardware/waveshare_esp32s3_touch_lcd_1_46.md#building-the-firmware)).

## Sounds on the SD card

1. Make a folder called `sounds` on the card.
2. Copy WAV, MP3 or FLAC files into it.
3. Put the card in the board and switch it on.

The sounds are played in order of file name.  They're read from the card as they play, so they can be any length.  A sound that repeats (*Repeat one*) starts again straight after its end, so short sound effects loop without a gap.  MP3s loop without a gap if the encoder recorded the silence it adds to each end (LAME and ffmpeg do); otherwise there's a short pause at the loop.

Limits:

- Up to 100 sounds are found on the card.
- Any sample rate works.  Mono and stereo files both play, and files with more channels play their first two.
- Files that can't be played are skipped, and the reason is printed in the board's log.
- The card is only read when the board starts, so restart it after changing the card.

While the [Voice Effect](voice-effects.md) is playing on the speaker, the player waits.
