#ifndef RCAT_LOVENSE_H
#define RCAT_LOVENSE_H

/*
 * Controls Lovense devices over Bluetooth LE: finds them, connects to up to
 * a few at once, works out which model each one is, and sets each of its
 * motors. Everything runs from lovense_poll(), which the UI calls regularly,
 * so nothing here needs a task of its own.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LOVENSE_MAX_DEVICES     8   // found or connected, listed together
#define LOVENSE_MAX_CONNECTED   3
#define LOVENSE_MAX_MOTORS      3

typedef enum
{
    LOVENSE_VIBRATE,
    LOVENSE_ROTATE,
    LOVENSE_AIR,        // the Max's air pump
    LOVENSE_OSCILLATE,  // thrusting or stroking
} lovense_motor_kind_t;

typedef enum
{
    LOVENSE_FOUND,          // seen in the scan, not connected
    LOVENSE_CONNECTING,     // connecting, or asking which model it is
    LOVENSE_CONNECTED,
    LOVENSE_FAILED,         // couldn't connect
    LOVENSE_LOST,           // the connection dropped
} lovense_device_state_t;

typedef struct
{
    lovense_motor_kind_t kind;
    uint8_t max;            // levels run from 0 to this
    uint8_t level;
} lovense_motor_t;

typedef struct
{
    char name[32];              // advertised name, e.g. "LVS-Edge"
    const char *model;          // e.g. "Edge", once known; otherwise the advertised name
    lovense_device_state_t state;
    int battery;                // percent, or -1 if it hasn't said yet
    uint8_t motor_count;        // 0 until it's connected
    lovense_motor_t motors[LOVENSE_MAX_MOTORS];
} lovense_device_t;

/* Start or stop looking for devices. Starting forgets the devices that aren't
 * connected. Returns false if the board has no Bluetooth. */
bool lovense_scan_start(void);
void lovense_scan_stop(void);
bool lovense_scanning(void);

/* The devices, found or connected, by slot. A slot keeps its device until the
 * next scan starts, unless it's connected. Returns NULL for an empty slot. */
const lovense_device_t *lovense_device(size_t slot);

/* Connect to a found device, or stop and disconnect a connected one. Returns
 * false if it can't connect now: too many connected, or one is still
 * connecting. */
bool lovense_toggle(size_t slot);

/* Set one motor, 0 to its max. It's sent on the next poll. Levels go back to
 * 0 when a device disconnects. */
void lovense_set_level(size_t slot, uint8_t motor, uint8_t level);

/* Set every motor of every connected device to 0. */
void lovense_stop_all(void);

/* Run the connections. Call every 50 ms or so with a millisecond clock. */
void lovense_poll(uint32_t now_ms);

/* Short name for a kind of motor, e.g. "Vibrate". */
const char *lovense_motor_name(lovense_motor_kind_t kind);

#endif
