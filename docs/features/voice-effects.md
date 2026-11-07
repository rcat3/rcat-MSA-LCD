# Voice Effects

*Boards with a microphone: the ESP32-S3 boards*

The *Voice Effect* page, in the Voice feature, runs your voice through an effect on the board itself:

| Preset | Sound |
|---|---|
| Female | Your own intonation, raised in pitch, with the vowels raised less, for a natural female voice |
| Fembot | Female, with the pitch snapped to musical notes and a light chorus |
| Synthetic | Snapped pitch, a faint metallic ring and slight digital grit |
| Android | A monotone female robot |
| AI | Thinner, higher and snapped, with a quick shimmer |
| Robot | A monotone robot at a low pitch |
| Deep | Your voice, lower |
| Dalek | Ring modulated |
| Drone | Lower, ring modulated and slightly crushed |

The effect goes to the [Voice Lab](voice-lab.md) stream (`voice_monitor.py --listen effect` plays it) and, with *Play on speaker* switched on, to the board's speaker.  While it's on the speaker, the [sound player](sound.md) waits.

*Play on speaker* is always off at startup: with the speaker facing into the mask, it feeds back.  A speaker facing away from the mask works much better, like the ones in the speaker housings (see [Hardware](../../README.md#hardware)).

The presets are the same as the effects in `tools/voice_monitor.py`, so they can be tried and tuned on a computer first.  The pitch-following presets add about 15-20 ms of delay, the others less.
