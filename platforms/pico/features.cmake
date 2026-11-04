# Which features to build for the RP2040 and RP2350 boards: ON to build one
# in, OFF to leave it out. See core/features.cmake.

set(RCAT_FEATURE_IMAGES ON)     # images and animations
set(RCAT_FEATURE_VOICE  ON)     # microphone level displays and voice settings
set(RCAT_FEATURE_SOUND  ON)     # sound file player
set(RCAT_FEATURE_LEVEL  OFF)    # spirit level. These boards' accelerometer isn't supported yet
set(RCAT_FEATURE_LOVENSE OFF)   # Lovense control. These boards have no Bluetooth
set(RCAT_FEATURE_TERMINAL OFF)  # text from the phone remote. These boards have no WiFi
