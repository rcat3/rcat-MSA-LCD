#ifndef RCAT_PATTERNS_H
#define RCAT_PATTERNS_H

/*
 * Patterns for the Lovense feature: levels that change over time, for each
 * motor. A few are built in, and more can be put on the SD card, in a folder
 * called "patterns", as text files in Lovense's pattern format:
 *
 *   V:1;F:v;S:100#0;4;8;12;16;20;16;12;8;4
 *
 * F lists the motors the levels drive: v vibrate, r rotate, p the air pump,
 * t, f or o thrusting (a number after the letter picks one motor of that
 * kind, e.g. v2 for the second vibrator; no F drives them all). S is the time
 * each level lasts, in milliseconds (100 or more). The levels run from 0 to
 * 20 and repeat. A file can hold several of these, one after another, each
 * driving its own motors and repeating at its own length.
 */

#include <stddef.h>
#include <stdint.h>

#include "app/lovense.h"

#define PATTERNS_FOLDER     "patterns"

/* Load the built-in patterns, then any on the card. storage_path is where the
 * card is mounted, or NULL without one. */
void patterns_init(const char *storage_path);

size_t patterns_count(void);
const char *patterns_name(size_t index);

/* How hard a motor should run, 0 to 1, t_ms into the pattern. which is 1 for
 * the first motor of its kind on the device, 2 for the second and so on, and
 * motor is its place among all the device's motors. Returns -1 if the pattern
 * doesn't drive this motor. */
float patterns_level(size_t index, lovense_motor_kind_t kind, uint8_t which, uint8_t motor, uint32_t t_ms);

#endif
