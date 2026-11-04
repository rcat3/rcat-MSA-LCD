/*
 * Lovense devices over Bluetooth LE.
 *
 * They advertise names starting "LVS-" (or "LOVE-" on some older ones), and
 * take plain text commands ending in ';' on a serial-style service, with
 * replies sent back as notifications:
 *
 *   DeviceType;    replies "<model>:<firmware>:<address>;", e.g. "P:37:..."
 *   Battery;       replies with the percentage, e.g. "85;" (some put an 's'
 *                  in front of it)
 *   Vibrate:N;     level 0 to 20 on single motor devices, replies "OK;"
 *   Vibrate1:N;    and Vibrate2:N; for each motor on two motor devices
 *   Rotate:N;      0 to 20 on devices that rotate
 *   Air:Level:N;   0 to 3 on the Max's air pump
 *   Mply:a:b:c;    all the motors at once, on the Lapis and newer Flexer
 *
 * The services, commands and model list are from the Buttplug project's
 * device configuration (https://github.com/buttplugio/buttplug).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/lovense.h"
#include "app/patterns.h"
#include "hal.h"

#define SEND_GAP_MS         100     // at most one command per device this often
#define IDENTIFY_EVERY_MS   500     // ask which model it is this often
#define IDENTIFY_TRIES      5       // before going by its advertised name instead
#define BATTERY_EVERY_MS    30000   // also keeps the connection busy, which some devices want
#define STOP_SETTLE_MS      200     // time for the stop to go out before disconnecting

/* How a model takes its commands */
typedef enum
{
    STYLE_SINGLE,       // Vibrate:N;
    STYLE_DUAL,         // Vibrate1:N; Vibrate2:N;
    STYLE_MAX,          // Vibrate:N; Air:Level:N;
    STYLE_ROTATE,       // Vibrate:N; Rotate:N;
    STYLE_MULTI,        // Mply:a:b:c;
    STYLE_STROKER,      // Mply:N:20;
} style_t;

typedef struct
{
    const char *id;     // what DeviceType replies with
    const char *model;
    style_t style;
    uint8_t motor_count;
    lovense_motor_kind_t kinds[LOVENSE_MAX_MOTORS];
} model_t;

#define V   LOVENSE_VIBRATE
#define R   LOVENSE_ROTATE
#define O   LOVENSE_OSCILLATE

static const model_t models[] = {
    { "S",    "Lush",              STYLE_SINGLE,  1, { V } },
    { "AN",   "Lush Anal",         STYLE_SINGLE,  1, { V } },
    { "Z",    "Hush",              STYLE_SINGLE,  1, { V } },
    { "L",    "Ambi",              STYLE_SINGLE,  1, { V } },
    { "ZA",   "Synth",             STYLE_SINGLE,  1, { V } },
    { "W",    "Domi",              STYLE_SINGLE,  1, { V } },
    { "O",    "Osci",              STYLE_SINGLE,  1, { V } },
    { "V",    "Mission",           STYLE_SINGLE,  1, { V } },
    { "CA",   "Mission 2",         STYLE_SINGLE,  1, { V } },
    { "X",    "Ferri",             STYLE_SINGLE,  1, { V } },
    { "R",    "Diamo",             STYLE_SINGLE,  1, { V } },
    { "ED",   "Gush",              STYLE_SINGLE,  1, { V } },
    { "EZ",   "Gush 2",            STYLE_SINGLE,  1, { V } },
    { "T",    "Calor",             STYLE_SINGLE,  1, { V } },
    { "Q",    "Tenera",            STYLE_SINGLE,  1, { V } },
    { "SD",   "Vulse",             STYLE_SINGLE,  1, { V } },
    { "EI",   "Flexer",            STYLE_SINGLE,  1, { V } },   // older firmware
    { "ToyS", "Dolp",              STYLE_SINGLE,  1, { V } },
    { "QB",   "Fizz",              STYLE_SINGLE,  1, { V } },
    { "F",    "Sex Machine",       STYLE_SINGLE,  1, { O } },
    { "FS",   "Mini Sex Machine",  STYLE_SINGLE,  1, { O } },
    { "P",    "Edge",              STYLE_DUAL,    2, { V, V } },
    { "J",    "Dolce",             STYLE_DUAL,    2, { V, V } },
    { "OC",   "Osci 3",            STYLE_DUAL,    2, { V, V } },
    { "EB",   "Hyphy",             STYLE_DUAL,    2, { V, V } },
    { "N",    "Gemini",            STYLE_DUAL,    2, { V, V } },
    { "EA",   "Gravity",           STYLE_DUAL,    2, { V, O } },
    { "WD",   "Spinel",            STYLE_DUAL,    2, { V, O } },
    { "B",    "Max",               STYLE_MAX,     2, { V, LOVENSE_AIR } },
    { "A",    "Nora",              STYLE_ROTATE,  2, { V, R } },
    { "C",    "Nora",              STYLE_ROTATE,  2, { V, R } },
    { "CB",   "Velvo",             STYLE_ROTATE,  2, { V, R } },
    { "EL",   "Ridge",             STYLE_ROTATE,  2, { V, R } },
    { "U",    "Lapis",             STYLE_MULTI,   3, { V, V, V } },
    { "EI3",  "Flexer",            STYLE_MULTI,   3, { V, V, R } },   // firmware 3 and later
    { "H",    "Solace",            STYLE_STROKER, 1, { O } },
    { "BA",   "Solace Pro",        STYLE_STROKER, 1, { O } },
};

#undef V
#undef R
#undef O

// For a model that isn't in the list
static const model_t unknown_model = { "", NULL, STYLE_SINGLE, 1, { LOVENSE_VIBRATE } };

typedef struct
{
    lovense_device_t pub;           // what lovense_device() hands out
    bool used;
    rcat_hal_ble_device_t ble;
    int link;                       // HAL link while connecting or connected, else -1
    const model_t *model_info;      // NULL until it's known
    uint8_t sent[LOVENSE_MAX_MOTORS];
    bool unsent;                    // a level has changed since it was last sent
    uint8_t identify_tries;
    uint32_t last_send_ms;
    uint32_t last_identify_ms;
    uint32_t last_battery_ms;
    uint32_t stop_ms;               // when it was told to stop before disconnecting, or 0
    uint32_t pattern_ms;            // how far into its pattern it is, at the pattern's own speed
    bool battery_asked;
    char reply[32];
    size_t reply_len;
} slot_t;

static slot_t slots[LOVENSE_MAX_DEVICES];
static bool scanning;
static uint32_t now;                // the time of the last poll
static uint32_t last_poll;
static uint16_t speed = 100;        // percent
static bool reactive;
static float reactive_factor = 1;

/* XXXX00?1-002?-4bd4-bbd5-a6920e4c5653 (the usual one), the Nordic UART
 * service, or 0000fff0 on some older devices. */
static bool is_lovense_service(const uint8_t u[16])
{
    static const uint8_t lovense_tail[10] = { 0x4b, 0xd4, 0xbb, 0xd5, 0xa6, 0x92, 0x0e, 0x4c, 0x56, 0x53 };
    static const uint8_t nordic_uart[16] = { 0x6e, 0x40, 0x00, 0x01, 0xb5, 0xa3, 0xf3, 0x93,
                                             0xe0, 0xa9, 0xe5, 0x0e, 0x24, 0xdc, 0xca, 0x9e };
    static const uint8_t fff0[16] = { 0x00, 0x00, 0xff, 0xf0, 0x00, 0x00, 0x10, 0x00,
                                      0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb };

    if (memcmp(u + 6, lovense_tail, sizeof(lovense_tail)) == 0
        && u[2] == 0x00 && (u[3] & 0x0F) == 0x01
        && u[4] == 0x00 && (u[5] == 0x23 || u[5] == 0x24))
    {
        return true;
    }
    return memcmp(u, nordic_uart, 16) == 0 || memcmp(u, fff0, 16) == 0;
}

static bool is_lovense_name(const char *name)
{
    return strncmp(name, "LVS-", 4) == 0 || strncmp(name, "LOVE-", 5) == 0;
}

const char *lovense_motor_name(lovense_motor_kind_t kind)
{
    switch (kind)
    {
    case LOVENSE_ROTATE:    return "Rotate";
    case LOVENSE_AIR:       return "Air";
    case LOVENSE_OSCILLATE: return "Thrust";
    default:                return "Vibrate";
    }
}

static const model_t *find_model(const char *id)
{
    for (size_t i = 0; i < sizeof(models) / sizeof(models[0]); i++)
    {
        if (strcmp(models[i].id, id) == 0)
        {
            return &models[i];
        }
    }
    return NULL;
}

/* Take on a model's motors, all starting at 0. */
static void set_model(slot_t *s, const model_t *m)
{
    s->model_info = m;
    s->pub.model = m->model != NULL ? m->model : s->pub.name;
    s->pub.motor_count = m->motor_count;
    for (uint8_t i = 0; i < m->motor_count; i++)
    {
        s->pub.motors[i].kind = m->kinds[i];
        s->pub.motors[i].max = m->kinds[i] == LOVENSE_AIR ? 3 : 20;
        s->pub.motors[i].level = 0;
        s->pub.motors[i].cap = 0;
        s->sent[i] = 0;
    }
    s->unsent = false;
    s->pub.pattern = -1;
}

/* When DeviceType gets no answer: names like "LVS-Z36" carry the model's
 * letters before the digits. */
static const model_t *model_from_name(const char *name)
{
    const char *p = strchr(name, '-');
    if (p == NULL)
    {
        return NULL;
    }
    p++;
    char id[8];
    size_t n = 0;
    while (p[n] >= 'A' && p[n] <= 'Z' && n < sizeof(id) - 1)
    {
        id[n] = p[n];
        n++;
    }
    if (n == 0 || p[n] < '0' || p[n] > '9')
    {
        return NULL;
    }
    id[n] = '\0';
    return find_model(id);
}

/* ----- Sending ----- */

static bool send(slot_t *s, const char *command)
{
    if (!rcat_hal_ble_send(s->link, command, strlen(command)))
    {
        return false;
    }
    s->last_send_ms = now;
    return true;
}

/* The command that sets motor i to level, for this model. */
static void motor_command(const slot_t *s, uint8_t i, uint8_t level, char *out, size_t len)
{
    const model_t *m = s->model_info;
    switch (m->style)
    {
    case STYLE_DUAL:
        snprintf(out, len, "Vibrate%u:%u;", i + 1, level);
        break;
    case STYLE_MAX:
        snprintf(out, len, i == 0 ? "Vibrate:%u;" : "Air:Level:%u;", level);
        break;
    case STYLE_ROTATE:
        snprintf(out, len, i == 0 ? "Vibrate:%u;" : "Rotate:%u;", level);
        break;
    case STYLE_MULTI:
        snprintf(out, len, "Mply:%u:%u:%u;", s->pub.motors[0].level, s->pub.motors[1].level,
                 s->pub.motors[2].level);
        break;
    case STYLE_STROKER:
        // The Solace has to be told the stroke range is 0 to stop it.
        snprintf(out, len, "Mply:%u:%u;", level, (level == 0 && strcmp(m->id, "H") == 0) ? 0 : 20);
        break;
    default:
        snprintf(out, len, "Vibrate:%u;", level);
        break;
    }
}

/* Send every level that has changed, one after another. */
static void send_levels(slot_t *s)
{
    bool all_sent = true;
    for (uint8_t i = 0; i < s->pub.motor_count; i++)
    {
        uint8_t level = s->pub.motors[i].level;
        if (level == s->sent[i])
        {
            continue;
        }
        char command[24];
        motor_command(s, i, level, command, sizeof(command));
        if (!send(s, command))
        {
            all_sent = false;
            continue;
        }
        if (s->model_info->style == STYLE_MULTI)
        {
            // One command sets them all.
            for (uint8_t j = 0; j < s->pub.motor_count; j++)
            {
                s->sent[j] = s->pub.motors[j].level;
            }
            break;
        }
        s->sent[i] = level;
    }
    s->unsent = !all_sent;
}

/* Work out each motor's level from the pattern playing (if any) and the
 * reaction (if on), dt_ms after the last time. */
static void run_drive(slot_t *s, uint32_t dt_ms)
{
    s->pattern_ms += dt_ms * speed / 100;
    float factor = reactive ? reactive_factor : 1;
    for (uint8_t i = 0; i < s->pub.motor_count; i++)
    {
        lovense_motor_t *m = &s->pub.motors[i];
        float v = 1;
        if (s->pub.pattern >= 0)
        {
            uint8_t which = 0;
            for (uint8_t j = 0; j <= i; j++)
            {
                which += s->pub.motors[j].kind == m->kind;
            }
            v = patterns_level((size_t)s->pub.pattern, m->kind, which, i, s->pattern_ms);
        }
        uint8_t level = v < 0 ? 0 : (uint8_t)(v * factor * m->cap + 0.5f);
        if (level != m->level)
        {
            m->level = level;
            s->unsent = true;
        }
    }
}

/* Stop every motor straight away, ahead of disconnecting. */
static void send_stop(slot_t *s)
{
    if (s->model_info == NULL)
    {
        return;
    }
    s->pub.pattern = -1;
    for (uint8_t i = 0; i < s->pub.motor_count; i++)
    {
        s->pub.motors[i].level = 0;
    }
    for (uint8_t i = 0; i < s->pub.motor_count; i++)
    {
        char command[24];
        motor_command(s, i, 0, command, sizeof(command));
        send(s, command);
        s->sent[i] = 0;
        if (s->model_info->style == STYLE_MULTI)
        {
            break;      // one command does them all
        }
    }
}

/* ----- Replies ----- */

static void handle_reply(slot_t *s, const char *text)
{
    // DeviceType: "<model>:<firmware>:<address>"
    const char *colon = strchr(text, ':');
    if (colon != NULL && s->model_info == NULL && text[0] >= 'A' && text[0] <= 'Z')
    {
        char id[8];
        size_t n = (size_t)(colon - text);
        if (n < sizeof(id) - 1)
        {
            memcpy(id, text, n);
            id[n] = '\0';
            if (strcmp(id, "EI") == 0 && atoi(colon + 1) >= 3)
            {
                strcpy(id, "EI3");
            }
            const model_t *m = find_model(id);
            set_model(s, m != NULL ? m : &unknown_model);
        }
        return;
    }

    // Battery: a number, perhaps after an 's'
    const char *p = text[0] == 's' ? text + 1 : text;
    if (*p == '\0')
    {
        return;
    }
    int value = 0;
    for (; *p != '\0'; p++)
    {
        if (*p < '0' || *p > '9')
        {
            return;     // "OK", "ERR" and the like
        }
        value = value * 10 + (*p - '0');
    }
    if (s->battery_asked && value <= 100)
    {
        s->pub.battery = value;
        s->battery_asked = false;
    }
}

static void read_replies(slot_t *s)
{
    char data[64];
    size_t n;
    while ((n = rcat_hal_ble_receive(s->link, data, sizeof(data))) > 0)
    {
        for (size_t i = 0; i < n; i++)
        {
            if (data[i] == ';')
            {
                s->reply[s->reply_len] = '\0';
                handle_reply(s, s->reply);
                s->reply_len = 0;
            }
            else if (s->reply_len < sizeof(s->reply) - 1)
            {
                s->reply[s->reply_len++] = data[i];
            }
        }
    }
}

/* ----- Devices ----- */

static size_t connected_count(void)
{
    size_t n = 0;
    for (size_t i = 0; i < LOVENSE_MAX_DEVICES; i++)
    {
        if (slots[i].used && (slots[i].pub.state == LOVENSE_CONNECTING || slots[i].pub.state == LOVENSE_CONNECTED))
        {
            n++;
        }
    }
    return n;
}

static void add_found_devices(void)
{
    rcat_hal_ble_device_t found[16];
    size_t count = rcat_hal_ble_scan_results(found, sizeof(found) / sizeof(found[0]));
    for (size_t i = 0; i < count; i++)
    {
        if (!is_lovense_name(found[i].name))
        {
            continue;
        }
        slot_t *free_slot = NULL;
        bool known = false;
        for (size_t j = 0; j < LOVENSE_MAX_DEVICES; j++)
        {
            if (slots[j].used && memcmp(slots[j].ble.addr, found[i].addr, 6) == 0)
            {
                known = true;
                break;
            }
            if (!slots[j].used && free_slot == NULL)
            {
                free_slot = &slots[j];
            }
        }
        if (known || free_slot == NULL)
        {
            continue;
        }
        memset(free_slot, 0, sizeof(*free_slot));
        free_slot->used = true;
        free_slot->ble = found[i];
        free_slot->link = -1;
        strncpy(free_slot->pub.name, found[i].name, sizeof(free_slot->pub.name) - 1);
        free_slot->pub.model = free_slot->pub.name;
        free_slot->pub.state = LOVENSE_FOUND;
        free_slot->pub.battery = -1;
        free_slot->pub.pattern = -1;
    }
}

bool lovense_scan_start(void)
{
    if (!rcat_hal_ble_scan_start())
    {
        return false;
    }
    for (size_t i = 0; i < LOVENSE_MAX_DEVICES; i++)
    {
        if (slots[i].link < 0)
        {
            slots[i].used = false;
        }
    }
    scanning = true;
    return true;
}

void lovense_scan_stop(void)
{
    if (scanning)
    {
        rcat_hal_ble_scan_stop();
        scanning = false;
    }
}

bool lovense_scanning(void)
{
    return scanning;
}

const lovense_device_t *lovense_device(size_t slot)
{
    return slot < LOVENSE_MAX_DEVICES && slots[slot].used ? &slots[slot].pub : NULL;
}

bool lovense_toggle(size_t slot)
{
    if (slot >= LOVENSE_MAX_DEVICES || !slots[slot].used)
    {
        return false;
    }
    slot_t *s = &slots[slot];
    switch (s->pub.state)
    {
    case LOVENSE_CONNECTING:
        rcat_hal_ble_disconnect(s->link);
        s->link = -1;
        s->pub.state = LOVENSE_FOUND;
        return true;

    case LOVENSE_CONNECTED:
        if (s->stop_ms == 0)
        {
            send_stop(s);
            s->stop_ms = now | 1;   // lovense_poll() disconnects once the stop has gone out
        }
        return true;

    default:
        if (connected_count() >= LOVENSE_MAX_CONNECTED)
        {
            return false;
        }
        s->link = rcat_hal_ble_connect(&s->ble, is_lovense_service);
        if (s->link < 0)
        {
            return false;
        }
        s->pub.state = LOVENSE_CONNECTING;
        s->pub.battery = -1;
        s->pub.motor_count = 0;
        s->pub.pattern = -1;
        s->model_info = NULL;
        s->identify_tries = 0;
        s->stop_ms = 0;
        s->reply_len = 0;
        return true;
    }
}

void lovense_set_level(size_t slot, uint8_t motor, uint8_t level)
{
    if (slot >= LOVENSE_MAX_DEVICES || !slots[slot].used || motor >= slots[slot].pub.motor_count)
    {
        return;
    }
    lovense_motor_t *m = &slots[slot].pub.motors[motor];
    if (level > m->max)
    {
        level = m->max;
    }
    if (lovense_driven(&slots[slot].pub))
    {
        m->cap = level;     // the next poll scales the pattern or reaction to it
        return;
    }
    m->level = level;
    slots[slot].unsent = true;
}

/* Caps for a pattern or reaction to drive up to: the level each motor was
 * at, or three quarters of the way up if it was off. */
static void start_caps(slot_t *s)
{
    for (uint8_t i = 0; i < s->pub.motor_count; i++)
    {
        lovense_motor_t *m = &s->pub.motors[i];
        m->cap = m->level > 0 ? m->level : (uint8_t)((m->max * 3 + 3) / 4);
    }
}

void lovense_play(size_t slot, int pattern)
{
    if (slot >= LOVENSE_MAX_DEVICES || !slots[slot].used || slots[slot].pub.state != LOVENSE_CONNECTED)
    {
        return;
    }
    slot_t *s = &slots[slot];
    if (pattern < 0 || (size_t)pattern >= patterns_count())
    {
        // Stopped. With a reaction on, it carries on at the caps.
        s->pub.pattern = -1;
        if (!reactive)
        {
            for (uint8_t i = 0; i < s->pub.motor_count; i++)
            {
                s->pub.motors[i].level = 0;
            }
            s->unsent = true;
        }
        return;
    }
    if (!lovense_driven(&s->pub))
    {
        start_caps(s);
    }
    s->pub.pattern = pattern;
    s->pattern_ms = 0;
}

void lovense_set_reactive(bool on)
{
    if (on == reactive)
    {
        return;
    }
    for (size_t i = 0; i < LOVENSE_MAX_DEVICES; i++)
    {
        slot_t *s = &slots[i];
        if (!s->used || s->pub.state != LOVENSE_CONNECTED || s->pub.pattern >= 0)
        {
            continue;   // a pattern already has its caps
        }
        if (on)
        {
            start_caps(s);
        }
        else
        {
            for (uint8_t j = 0; j < s->pub.motor_count; j++)
            {
                s->pub.motors[j].level = s->pub.motors[j].cap;
            }
            s->unsent = true;
        }
    }
    reactive = on;
}

bool lovense_reactive(void)
{
    return reactive;
}

void lovense_set_reactive_factor(float factor)
{
    reactive_factor = factor < 0 ? 0 : factor > 1 ? 1 : factor;
}

bool lovense_driven(const lovense_device_t *device)
{
    return device->pattern >= 0 || reactive;
}

void lovense_set_speed(uint16_t percent)
{
    speed = percent < 10 ? 10 : percent > 400 ? 400 : percent;
}

uint16_t lovense_speed(void)
{
    return speed;
}

void lovense_stop_all(void)
{
    for (size_t i = 0; i < LOVENSE_MAX_DEVICES; i++)
    {
        if (slots[i].used && slots[i].pub.pattern >= 0)
        {
            lovense_play(i, -1);
        }
        for (uint8_t j = 0; slots[i].used && j < slots[i].pub.motor_count; j++)
        {
            lovense_set_level(i, j, 0);
        }
    }
    lovense_set_reactive(false);    // the caps are 0 now, so the motors stay stopped
}

static void poll_device(slot_t *s)
{
    rcat_hal_ble_state_t ble = rcat_hal_ble_link_state(s->link);

    switch (s->pub.state)
    {
    case LOVENSE_CONNECTING:
        if (ble == RCAT_HAL_BLE_IDLE)
        {
            s->link = -1;
            s->pub.state = LOVENSE_FAILED;
            break;
        }
        if (ble != RCAT_HAL_BLE_CONNECTED)
        {
            break;
        }
        // Connected: find out which model it is, then it's ready.
        read_replies(s);
        if (s->model_info == NULL && now - s->last_identify_ms >= IDENTIFY_EVERY_MS)
        {
            if (s->identify_tries >= IDENTIFY_TRIES)
            {
                const model_t *m = model_from_name(s->pub.name);
                set_model(s, m != NULL ? m : &unknown_model);
            }
            else if (send(s, "DeviceType;"))
            {
                s->identify_tries++;
                s->last_identify_ms = now;
            }
        }
        if (s->model_info != NULL)
        {
            s->pub.state = LOVENSE_CONNECTED;
            s->last_battery_ms = now - BATTERY_EVERY_MS;    // ask straight away
        }
        break;

    case LOVENSE_CONNECTED:
        if (ble != RCAT_HAL_BLE_CONNECTED)
        {
            s->link = -1;
            s->stop_ms = 0;
            s->pub.state = LOVENSE_LOST;
            s->pub.pattern = -1;
            for (uint8_t i = 0; i < s->pub.motor_count; i++)
            {
                s->pub.motors[i].level = 0;     // so it doesn't start up again on reconnecting
            }
            break;
        }
        if (s->stop_ms != 0)
        {
            if (now - s->stop_ms >= STOP_SETTLE_MS)
            {
                rcat_hal_ble_disconnect(s->link);
                s->link = -1;
                s->stop_ms = 0;
                s->pub.state = LOVENSE_FOUND;
            }
            break;
        }
        read_replies(s);
        if (lovense_driven(&s->pub))
        {
            run_drive(s, now - last_poll);
        }
        if (now - s->last_send_ms < SEND_GAP_MS)
        {
            break;
        }
        // The battery first when it's due, as a pattern keeps the levels
        // changing.
        if (now - s->last_battery_ms >= BATTERY_EVERY_MS)
        {
            if (send(s, "Battery;"))
            {
                s->battery_asked = true;
                s->last_battery_ms = now;
            }
        }
        else if (s->unsent)
        {
            send_levels(s);
        }
        break;

    default:
        break;
    }
}

void lovense_poll(uint32_t now_ms)
{
    last_poll = now != 0 ? now : now_ms;
    now = now_ms;
    if (scanning)
    {
        add_found_devices();
    }
    for (size_t i = 0; i < LOVENSE_MAX_DEVICES; i++)
    {
        if (slots[i].used)
        {
            poll_device(&slots[i]);
        }
    }
}
