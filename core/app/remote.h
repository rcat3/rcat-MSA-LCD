#ifndef RCAT_REMOTE_H
#define RCAT_REMOTE_H

/*
 * Phone remote for the Lovense feature: the board runs a WiFi hotspot of its
 * own, and a phone that joins it gets a web page with the same controls as
 * the screen: the devices, their motors, patterns, reactions and Stop.
 *
 * Joining is meant to be easy: the screen shows a QR code that a phone's
 * camera turns into "join this network", and once it's joined, the phone
 * opens the page by itself, as it does for a hotel's WiFi sign-in. Failing
 * that, the page is at http://192.168.4.1.
 *
 * The hotspot's name and password are made up the first time, and kept. A
 * new password (remote_new_password) shuts out anyone who had the old one.
 *
 * Everything runs from remote_poll(), on the UI loop, like the Lovense
 * connections, so the page and the screen change the same things and each
 * shows what the other did.
 */

#include <stdbool.h>
#include <stdint.h>

/* True on boards with WiFi. */
bool remote_available(void);

/* Start or stop the hotspot and the page. Returns false if it couldn't
 * start. */
bool remote_start(void);
void remote_stop(void);
bool remote_running(void);

/* The hotspot's name and password, and what a phone's camera needs to join
 * it (for a QR code). */
const char *remote_ssid(void);
const char *remote_password(void);
const char *remote_join_text(void);

/* Make up a new password. If it's running, it restarts, and phones that have
 * joined are dropped. */
void remote_new_password(void);

/* How many phones have joined. */
int remote_phones(void);

/* Answer the phones. Call every 50 ms or so with a millisecond clock. */
void remote_poll(uint32_t now_ms);

#endif
