/*
 * Built-in patterns, and patterns loaded from the card (see patterns.h for
 * the file format).
 *
 * The built-in ones are worked out from the time as they play, so they cost
 * no memory. On a device with two motors, some run the second one half a
 * cycle behind the first, so the two take turns.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if RCAT_HAVE_FILESYSTEM
#include <dirent.h>
#include <strings.h>
#endif

#include "app/patterns.h"
#include "lvgl.h"

#define MAX_PATTERNS        32
#define MAX_NAME            24
#define MAX_BLOCKS          4       // separately repeating parts of one file
#define MAX_TARGETS         6       // motors one part can drive
#define MAX_STEPS           4096    // levels in one part
#define MAX_FILE_BYTES      32768
#define MIN_STEP_MS         100     // Lovense's own patterns go no faster

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef float (*generator_t)(uint8_t motor, float seconds);

typedef struct
{
    lovense_motor_kind_t kind;
    uint8_t which;          // 0 for every motor of the kind
} target_t;

typedef struct
{
    uint16_t step_ms;
    uint8_t target_count;   // 0 drives every motor
    target_t targets[MAX_TARGETS];
    uint16_t step_count;
    uint8_t *steps;         // 0 to 20
} block_t;

typedef struct
{
    char name[MAX_NAME];
    generator_t generator;  // for a built-in one, otherwise NULL
    uint8_t block_count;
    block_t blocks[MAX_BLOCKS];
} pattern_t;

static pattern_t patterns[MAX_PATTERNS];
static size_t pattern_count;

/* ----- Built-in patterns, each 0 to 1 over time ----- */

static float wrap(float x, float period)
{
    return x - period * floorf(x / period);
}

/* Half a cycle later on the second motor, so two motors take turns. */
static float offset(uint8_t motor, float period)
{
    return (motor % 2) ? period / 2 : 0;
}

static float gen_pulse(uint8_t motor, float s)
{
    return wrap(s + offset(motor, 0.8f), 0.8f) < 0.4f ? 1.0f : 0.0f;
}

static float gen_wave(uint8_t motor, float s)
{
    float p = wrap(s + offset(motor, 4.0f), 4.0f) / 4.0f;
    return 0.5f - 0.5f * cosf(2 * (float)M_PI * p);
}

static float gen_heartbeat(uint8_t motor, float s)
{
    // Two beats, the second softer, then a rest
    float p = wrap(s, 1.1f);
    float beat = expf(-p * 18.0f);
    if (p > 0.25f)
    {
        beat += 0.7f * expf(-(p - 0.25f) * 18.0f);
    }
    return beat > 1.0f ? 1.0f : beat;
}

static float gen_climb(uint8_t motor, float s)
{
    // Up over eight seconds, then drop back
    return wrap(s, 8.0f) / 8.0f;
}

static float gen_surge(uint8_t motor, float s)
{
    // Waves that grow for twenty seconds, then a short rest
    float p = wrap(s, 23.0f);
    if (p > 20.0f)
    {
        return 0.0f;
    }
    float swell = 0.25f + 0.75f * p / 20.0f;
    return swell * (0.55f + 0.45f * sinf(2 * (float)M_PI * (p + offset(motor, 1.5f)) / 1.5f));
}

/* Smooth noise: random levels a third of a second apart, eased between. */
static float noise_at(uint32_t n, uint8_t motor)
{
    uint32_t h = n * 2654435761u + motor * 40503u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return (h & 0xFFFF) / 65535.0f;
}

static float gen_wander(uint8_t motor, float s)
{
    float x = s * 3.0f;
    uint32_t n = (uint32_t)x;
    float f = x - n;
    f = f * f * (3 - 2 * f);
    return noise_at(n, motor) + (noise_at(n + 1, motor) - noise_at(n, motor)) * f;
}

static void add_builtin(const char *name, generator_t generator)
{
    if (pattern_count < MAX_PATTERNS)
    {
        pattern_t *p = &patterns[pattern_count++];
        memset(p, 0, sizeof(*p));
        strncpy(p->name, name, MAX_NAME - 1);
        p->generator = generator;
    }
}

/* ----- Pattern files ----- */

#if RCAT_HAVE_FILESYSTEM

static bool parse_target(const char *s, size_t len, target_t *t)
{
    if (len == 0)
    {
        return false;
    }
    switch (s[0])
    {
    case 'v': t->kind = LOVENSE_VIBRATE; break;
    case 'r': t->kind = LOVENSE_ROTATE; break;
    case 'p': t->kind = LOVENSE_AIR; break;
    case 't':
    case 'f':
    case 'o': t->kind = LOVENSE_OSCILLATE; break;
    default: return false;     // s (suction) and d (depth): nothing here has them
    }
    t->which = len > 1 ? (uint8_t)atoi(s + 1) : 0;
    return true;
}

/* The header, e.g. "V:1;F:v,r;S:100". */
static bool parse_header(const char *s, size_t len, block_t *b)
{
    b->step_ms = 1000;
    b->target_count = 0;
    bool any_target_listed = false;
    size_t i = 0;
    while (i < len)
    {
        size_t end = i;
        while (end < len && s[end] != ';')
        {
            end++;
        }
        if (end - i >= 2 && s[i + 1] == ':')
        {
            const char *value = s + i + 2;
            size_t value_len = end - i - 2;
            switch (s[i])
            {
            case 'S':
            case 's':
                b->step_ms = (uint16_t)LV_CLAMP(MIN_STEP_MS, atoi(value), 60000);
                break;
            case 'F':
            case 'f':
                for (size_t j = 0; j < value_len;)
                {
                    size_t k = j;
                    while (k < value_len && value[k] != ',')
                    {
                        k++;
                    }
                    any_target_listed |= k > j;
                    if (b->target_count < MAX_TARGETS && parse_target(value + j, k - j, &b->targets[b->target_count]))
                    {
                        b->target_count++;
                    }
                    j = k + 1;
                }
                break;
            default:
                break;  // V, the version
            }
        }
        i = end + 1;
    }
    // Only motors this doesn't support, e.g. suction: leave the part out.
    return !(any_target_listed && b->target_count == 0);
}

static bool load_file(const char *path, const char *name)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL)
    {
        return false;
    }
    char *text = lv_malloc(MAX_FILE_BYTES + 1);
    uint8_t *steps = lv_malloc(MAX_STEPS);
    if (text == NULL || steps == NULL)
    {
        fclose(f);
        lv_free(text);
        lv_free(steps);
        return false;
    }
    size_t len = fread(text, 1, MAX_FILE_BYTES, f);
    fclose(f);
    text[len] = '\0';

    pattern_t *p = &patterns[pattern_count];
    memset(p, 0, sizeof(*p));
    snprintf(p->name, MAX_NAME, "%s", name);

    // Each part: a header up to '#', then levels up to the next header.
    const char *c = text;
    while (*c != '\0' && p->block_count < MAX_BLOCKS)
    {
        const char *hash = strchr(c, '#');
        if (hash == NULL)
        {
            break;
        }
        // The header starts at the last "V:" before the '#'.
        const char *header = c;
        for (const char *h = c; h < hash; h++)
        {
            if ((h[0] == 'V' || h[0] == 'v') && h[1] == ':')
            {
                header = h;
            }
        }
        block_t b;
        bool wanted = parse_header(header, (size_t)(hash - header), &b);

        size_t count = 0;
        c = hash + 1;
        while (*c != '\0' && !((c[0] == 'V' || c[0] == 'v') && c[1] == ':'))
        {
            if (*c >= '0' && *c <= '9')
            {
                int value = (int)strtol(c, (char **)&c, 10);
                if (count < MAX_STEPS)
                {
                    steps[count++] = (uint8_t)LV_CLAMP(0, value, 20);
                }
            }
            else
            {
                c++;    // ';', spaces and line breaks
            }
        }
        if (wanted && count > 0)
        {
            b.step_count = (uint16_t)count;
            b.steps = lv_malloc(count);
            if (b.steps != NULL)
            {
                memcpy(b.steps, steps, count);
                p->blocks[p->block_count++] = b;
            }
        }
    }
    lv_free(text);
    lv_free(steps);

    if (p->block_count == 0)
    {
        printf("patterns: nothing to play in %s\n", path);
        return false;
    }
    pattern_count++;
    return true;
}

static int compare_names(const void *a, const void *b)
{
    return strcasecmp(*(const char *const *)a, *(const char *const *)b);
}

static void load_folder(const char *storage_path)
{
    char folder[128];
    snprintf(folder, sizeof(folder), "%s/%s", storage_path, PATTERNS_FOLDER);
    DIR *dir = opendir(folder);
    if (dir == NULL)
    {
        return;
    }
    char *names[MAX_PATTERNS];
    size_t name_count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && name_count < MAX_PATTERNS)
    {
        const char *dot = strrchr(entry->d_name, '.');
        if (entry->d_name[0] != '.' && dot != NULL && (strcasecmp(dot, ".txt") == 0 || strcasecmp(dot, ".pat") == 0))
        {
            names[name_count++] = strdup(entry->d_name);
        }
    }
    closedir(dir);
    qsort(names, name_count, sizeof(names[0]), compare_names);

    for (size_t i = 0; i < name_count; i++)
    {
        if (names[i] == NULL)
        {
            continue;
        }
        if (pattern_count < MAX_PATTERNS)
        {
            char path[192];
            char name[MAX_NAME];
            snprintf(path, sizeof(path), "%s/%s", folder, names[i]);
            snprintf(name, sizeof(name), "%.*s", (int)(strrchr(names[i], '.') - names[i]), names[i]);
            if (load_file(path, name))
            {
                printf("patterns: loaded %s\n", names[i]);
            }
        }
        free(names[i]);
    }
}

#endif  // RCAT_HAVE_FILESYSTEM

void patterns_init(const char *storage_path)
{
    pattern_count = 0;
    add_builtin("Wave", gen_wave);
    add_builtin("Pulse", gen_pulse);
    add_builtin("Heartbeat", gen_heartbeat);
    add_builtin("Climb", gen_climb);
    add_builtin("Surge", gen_surge);
    add_builtin("Wander", gen_wander);
#if RCAT_HAVE_FILESYSTEM
    if (storage_path != NULL)
    {
        load_folder(storage_path);
    }
#else
    (void)storage_path;
#endif
}

size_t patterns_count(void)
{
    return pattern_count;
}

const char *patterns_name(size_t index)
{
    return index < pattern_count ? patterns[index].name : "";
}

static bool block_drives(const block_t *b, lovense_motor_kind_t kind, uint8_t which)
{
    if (b->target_count == 0)
    {
        return true;
    }
    for (uint8_t i = 0; i < b->target_count; i++)
    {
        if (b->targets[i].kind == kind && (b->targets[i].which == 0 || b->targets[i].which == which))
        {
            return true;
        }
    }
    return false;
}

float patterns_level(size_t index, lovense_motor_kind_t kind, uint8_t which, uint8_t motor, uint32_t t_ms)
{
    if (index >= pattern_count)
    {
        return -1;
    }
    const pattern_t *p = &patterns[index];
    if (p->generator != NULL)
    {
        return p->generator(motor, t_ms / 1000.0f);
    }

    // The last part that drives this motor wins.
    float level = -1;
    for (uint8_t i = 0; i < p->block_count; i++)
    {
        const block_t *b = &p->blocks[i];
        if (block_drives(b, kind, which))
        {
            uint32_t step = (t_ms / b->step_ms) % b->step_count;
            level = b->steps[step] / 20.0f;
        }
    }
    return level;
}
