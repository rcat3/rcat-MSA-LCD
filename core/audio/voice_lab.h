#ifndef RCAT_VOICE_LAB_H
#define RCAT_VOICE_LAB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * The voice lab streams the microphone over WiFi to tools/voice_monitor.py on
 * a computer, for trying out voice effects. The board joins the network
 * named in wifi.txt on the SD card:
 *
 *     ssid=My Network
 *     password=my password
 *
 * The monitor broadcasts a hello on VOICE_LAB_PORT about once a second, and
 * the board streams to whoever said hello last. The stream stops a few
 * seconds after the hellos do.
 */
#define VOICE_LAB_PORT 5005

/* What a stream carries: the microphone as it is, or after the voice effect. */
#define VOICE_LAB_KIND_MIC      0
#define VOICE_LAB_KIND_EFFECT   1

/* Remember where the SD card is, for wifi.txt. storage_path is NULL without
 * a card. */
void voice_lab_init(const char *storage_path);

/* True on boards that can stream (WiFi and a microphone). */
bool voice_lab_available(void);

/* Join the network and start listening for the monitor. Returns NULL, or a
 * message saying why it couldn't start. */
const char *voice_lab_start(void);
void voice_lab_stop(void);

/* Check for hellos from the monitor. Call it from the UI loop a few times a
 * second while the voice lab is on. */
void voice_lab_poll(uint32_t now_ms);

/* Send a block of samples (10 ms at most) to the monitor, if one is
 * listening. Call it from the microphone's task. */
void voice_lab_send(uint16_t kind, uint32_t sample_rate, const int16_t *samples, size_t count);

/* What it's doing, for the screen, e.g. "192.168.1.23\nStreaming to
 * 192.168.1.10". */
void voice_lab_status(char *text, size_t len);

#endif
