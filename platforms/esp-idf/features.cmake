# Which features to build for the ESP32 boards: ON to build one in, OFF to
# leave it out. See core/features.cmake.

set(RCAT_FEATURE_IMAGES ON)     # images and animations
set(RCAT_FEATURE_VOICE  ON)     # microphone level displays, voice effects and the voice lab
set(RCAT_FEATURE_SOUND  ON)     # sound file player
set(RCAT_FEATURE_LEVEL  ON)     # spirit level, for lining the display up on the mask
set(RCAT_FEATURE_LOVENSE ON)    # controls a Lovense device over Bluetooth

# Where the sound goes: BUILT_IN for the board's own speaker, or EXTERNAL for
# amplifiers wired to its expansion header (see hardware/enclosure/README.md).
set(RCAT_SPEAKER EXTERNAL)
