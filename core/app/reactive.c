/*
 * Reactions (see reactive.h).
 *
 * The accelerometer gives gravity plus any movement. A slow average of it is
 * taken as gravity, which gives the tilt, and how far each reading is from
 * that average is the movement.
 */
#include <math.h>
#include <stdio.h>

#include "app/reactive.h"
#include "audio/audio_level.h"
#include "hal.h"

#define GRAVITY_TAU_MS      500     // how slowly the gravity estimate follows the readings
#define ACTIVITY_FALL_MS    400     // how quickly the movement measure falls back after a move
#define STILL_G             0.03f   // less movement than this (in g) counts as still
#define STILL_TILT_DEG      10.0f   // and within this of upright
#define STILL_HOLD_MS       1000    // for this long
#define MOVING_G            0.06f   // more than this counts as moving
#define MOVING_HOLD_MS      300     // and keeps counting this long after (on top of the half second the movement takes to die away)
#define MOTION_FLOOR_G      0.02f   // Motion starts above this
#define MOTION_FULL_G       0.30f   // and is full at this
#define TILT_FLOOR_DEG      5.0f    // Tilt starts past this
#define TILT_FULL_DEG       35.0f   // and is full at this
#define RISE                0.35f   // how much of the way to the target the output moves each poll, going up
#define FALL                0.20f   // and going down

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static reactive_mode_t mode = REACTIVE_OFF;
static uint32_t last_ms;
static bool have_gravity;
static float gx, gy, gz;            // the gravity estimate, in g
static float activity;              // movement, in g
static float tilt_deg;
static uint32_t still_since;        // when it last became still, or 0
static uint32_t last_move_ms;
static bool accel_ok;
static float output;

static const char *const names[REACTIVE_COUNT] = {
    "Off", "Voice", "Moving", "Still", "Motion", "Tilt",
};

static float clamp01(float x)
{
    return x < 0 ? 0 : x > 1 ? 1 : x;
}

void reactive_set_mode(reactive_mode_t new_mode)
{
    if (new_mode >= REACTIVE_COUNT)
    {
        new_mode = REACTIVE_OFF;
    }
    mode = new_mode;
    output = 0;
    still_since = 0;
    last_move_ms = 0;
}

reactive_mode_t reactive_mode(void)
{
    return mode;
}

bool reactive_available(reactive_mode_t m)
{
    float x, y, z;
    switch (m)
    {
    case REACTIVE_OFF:
        return true;
    case REACTIVE_VOICE:
        return audio_level_available();
    case REACTIVE_COUNT:
        return false;
    default:
        return rcat_hal_accel_read(&x, &y, &z);
    }
}

const char *reactive_name(reactive_mode_t m)
{
    return m < REACTIVE_COUNT ? names[m] : "";
}

/* Take a reading and update the gravity estimate, the tilt and the movement. */
static void read_motion(uint32_t now_ms, uint32_t dt_ms)
{
    float x, y, z;
    accel_ok = rcat_hal_accel_read(&x, &y, &z);
    if (!accel_ok)
    {
        return;
    }
    if (!have_gravity)
    {
        gx = x;
        gy = y;
        gz = z;
        have_gravity = true;
    }
    float follow = (float)dt_ms / (dt_ms + GRAVITY_TAU_MS);
    gx += (x - gx) * follow;
    gy += (y - gy) * follow;
    gz += (z - gz) * follow;

    float dx = x - gx, dy = y - gy, dz = z - gz;
    float moved = sqrtf(dx * dx + dy * dy + dz * dz);
    if (moved > activity)
    {
        activity += (moved - activity) * 0.6f;   // up quickly
    }
    else
    {
        activity += (moved - activity) * ((float)dt_ms / (dt_ms + ACTIVITY_FALL_MS));
    }

    // Upright reads (0, -1, 0): the angle from that
    float g = sqrtf(gx * gx + gy * gy + gz * gz);
    float c = g > 0.1f ? -gy / g : 0;
    tilt_deg = acosf(c < -1 ? -1 : c > 1 ? 1 : c) * 180.0f / (float)M_PI;

    if (activity < STILL_G && tilt_deg < STILL_TILT_DEG)
    {
        if (still_since == 0)
        {
            still_since = now_ms | 1;
        }
    }
    else
    {
        still_since = 0;
    }
    if (activity > MOVING_G)
    {
        last_move_ms = now_ms | 1;
    }
}

float reactive_poll(uint32_t now_ms)
{
    uint32_t dt = last_ms != 0 ? now_ms - last_ms : 50;
    last_ms = now_ms;
    if (dt > 500)
    {
        dt = 500;
    }
    if (mode == REACTIVE_OFF)
    {
        return 1;
    }

    float target = 0;
    if (mode == REACTIVE_VOICE)
    {
        target = audio_level_get() / (float)AUDIO_LEVEL_MAX;
    }
    else
    {
        read_motion(now_ms, dt);
        if (accel_ok)
        {
            switch (mode)
            {
            case REACTIVE_MOVING:
                target = (last_move_ms != 0 && now_ms - last_move_ms < MOVING_HOLD_MS) ? 1 : 0;
                break;
            case REACTIVE_STILL:
                target = (still_since != 0 && now_ms - still_since >= STILL_HOLD_MS) ? 1 : 0;
                break;
            case REACTIVE_MOTION:
                target = clamp01((activity - MOTION_FLOOR_G) / (MOTION_FULL_G - MOTION_FLOOR_G));
                break;
            case REACTIVE_TILT:
                target = clamp01((tilt_deg - TILT_FLOOR_DEG) / (TILT_FULL_DEG - TILT_FLOOR_DEG));
                break;
            default:
                break;
            }
        }
    }

    output += (target - output) * (target > output ? RISE : FALL);
    if (output < 0.01f)
    {
        output = 0;
    }
    return output;
}

void reactive_status(char *text, size_t len)
{
    int percent = (int)(output * 100 + 0.5f);
    switch (mode)
    {
    case REACTIVE_OFF:
        snprintf(text, len, "Off");
        break;
    case REACTIVE_VOICE:
        snprintf(text, len, "Voice %d%%", percent);
        break;
    case REACTIVE_STILL:
        if (!accel_ok)
        {
            snprintf(text, len, "No accelerometer");
        }
        else if (percent > 0)
        {
            snprintf(text, len, "Still " "\xEF\x80\x8C");     // LV_SYMBOL_OK, a tick
        }
        else if (tilt_deg >= STILL_TILT_DEG)
        {
            snprintf(text, len, "Stand up straight");
        }
        else
        {
            snprintf(text, len, "Hold still...");
        }
        break;
    case REACTIVE_MOVING:
        snprintf(text, len, accel_ok ? (percent > 0 ? "Moving" : "Waiting for you to move") : "No accelerometer");
        break;
    case REACTIVE_MOTION:
        snprintf(text, len, accel_ok ? "Motion %d%%" : "No accelerometer", percent);
        break;
    case REACTIVE_TILT:
        snprintf(text, len, accel_ok ? "Tilt %d\xC2\xB0" : "No accelerometer", (int)(tilt_deg + 0.5f));
        break;
    default:
        text[0] = '\0';
        break;
    }
}
