# Voice Lab

*Boards with a microphone and WiFi: the ESP32-S3 boards*

The Voice Lab streams the board's microphone over WiFi to `tools/voice_monitor.py` on a computer, which plays it, records it and runs it through voice effects.  It's for trying out effects with the real microphone in the mask before building them into the firmware (see [Voice Effects](voice-effects.md)).

1. Put a file called `wifi.txt` on the SD card with your WiFi network's name and password (2.4 GHz networks only):
   ```
   ssid=My Network
   password=my password
   ```
2. Install the monitor's packages on the computer: `pip install numpy sounddevice` (on Linux, sounddevice also needs PortAudio: `sudo apt install libportaudio2`).
3. On the board, go to the last page of the Voice feature and switch the Voice Lab on.  It shows the board's address once it's connected.
4. On the computer, run `python3 tools/voice_monitor.py`.  It finds the board on the network, and the board shows *Streaming to* the computer's address.  If it isn't found, give the board's address: `python3 tools/voice_monitor.py --board 192.168.1.23`.

Some things to try:

- `--effect robot` (or `deep`, `dalek`, `drone`, `android`) plays the microphone through an effect.  Effects can be chained, like `--effect gate:-45,pitch:0.8,ring:40`.  Run with `--help` for the list.
- `--record take1.wav` saves the microphone as it is.  Then `--input take1.wav --effect robot` tries effects on the recording, without the board.
- `--listen effect` plays the board's own voice effect instead of the microphone.
- `--meter` prints the microphone level every second.  Play something on the speaker (the [Sound](sound.md) feature) to measure how much the microphone picks up.

WiFi adds a delay of around a tenth of a second, which is fine for judging an effect but odd if you listen to yourself live.  It's easier to have someone else listen, or to record and play back.

The Voice Lab is off whenever the board starts, and WiFi is off while it's off.  The Voice Lab and the [Phone Remote](phone-remote.md) both use WiFi, so they can't run at once: switching one on stops the other.
