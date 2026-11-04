#ifndef RCAT_REACTIVE_H
#define RCAT_REACTIVE_H

/*
 * Reactions for the Lovense feature: a factor from 0 to 1, worked out from
 * the microphone or the accelerometer, that scales whatever's driving the
 * motors (a pattern, or the levels set on the rings).
 *
 *   Voice    follows how loud you're talking
 *   Moving   on while you move, off soon after you stop
 *   Still    on only once you've held still, with the screen upright
 *   Motion   the more you move, the stronger
 *   Tilt     the further you tilt from upright, the stronger
 *
 * "Upright" is the picture standing upright, as the Level feature measures
 * it, so set the display's rotation first.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
    REACTIVE_OFF,
    REACTIVE_VOICE,
    REACTIVE_MOVING,
    REACTIVE_STILL,
    REACTIVE_MOTION,
    REACTIVE_TILT,
    REACTIVE_COUNT
} reactive_mode_t;

void reactive_set_mode(reactive_mode_t mode);
reactive_mode_t reactive_mode(void);

/* Whether the board has what a mode needs (a microphone or an
 * accelerometer). */
bool reactive_available(reactive_mode_t mode);

const char *reactive_name(reactive_mode_t mode);

/* Read the sensors and return the factor, 0 to 1. Call every 50 ms or so
 * with a millisecond clock. Returns 1 when the mode is off. */
float reactive_poll(uint32_t now_ms);

/* A few words on what it's sensing now, e.g. "Hold still..." or
 * "Tilt 12°", for showing on screen. */
void reactive_status(char *text, size_t len);

#endif
