# Features

The display is organized into features.  You swipe up and down to move between them, and left and right between each feature's pages (see [Using the Display](../using-the-display.md)).  Each feature can be left out of the build (see [Choosing features](../building-firmware.md#choosing-features)), and a feature that needs hardware the board doesn't have hides itself.

| Feature | What it does | RP2040 1.28" | RP2350 1.28" | ESP32-S3 1.46" | ESP32-S3 1.75" |
|---|---|:-:|:-:|:-:|:-:|
| [Images](images.md) | Shows still images and animated GIFs that you swipe through | Yes | Yes | Yes | Yes |
| [Images on the SD card](images.md#images-on-the-sd-card) | Adds images from a micro SD card, without rebuilding the firmware | | | Yes | Yes |
| [KITT and the Voice Ring](kitt-and-voice-ring.md) | Bars or a glowing ring that light up with your voice | | | Yes | Yes |
| [Voice Effects](voice-effects.md) | Changes your voice: female, robot, Dalek and more | | | Yes | Yes |
| [Voice Lab](voice-lab.md) | Streams the microphone to a computer, for trying out voice effects | | | Yes | Yes |
| [Sound](sound.md) | Plays sound files from the SD card | | | Yes | Yes |
| [Lovense](lovense.md) | Controls up to three Lovense devices over Bluetooth | | | Yes | Yes |
| [Level](level.md) | A spirit level, for lining the display up on the mask | | | Yes | Yes |
| [Terminal](terminal.md) | Shows text typed on a phone, like a green screen terminal | | | Yes | Yes |
| [Phone Remote](phone-remote.md) | Runs the display from a phone's web browser | | | Yes | Yes |
| [Settings](settings.md) | Brightness, rotation, what's shown at startup, and the battery voltage | Yes | Yes | Yes | Yes |

The ESP32-S3-Touch-AMOLED-1.75 is supported by the firmware, but hasn't been tried on a real board yet.

The RP2040 and RP2350 boards have no microphone, speaker, SD card slot, WiFi or Bluetooth, so they show images and settings.  They do have an accelerometer, which the firmware doesn't support yet, so the Level isn't on them.  The RP2350 has twice the RAM of the RP2040, enough for full screen animated GIFs (see [Animated GIF memory use](images.md#animated-gif-memory-use)).
