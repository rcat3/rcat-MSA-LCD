/*
 * The Terminal feature's text (see message.h).
 */
#include <string.h>

#include "app/message.h"

static char text[MESSAGE_MAX + 1];
static size_t text_len;
static uint32_t revision;

/* The next character of UTF-8 text as ASCII, moving *at on past it. Returns
 * 0 for something to leave out. */
static const char *to_ascii(const unsigned char *s, size_t len, size_t *at)
{
    unsigned char c = s[*at];
    if (c < 0x80)
    {
        (*at)++;
        if (c == '\n')
        {
            return "\n";
        }
        if (c == '\t')
        {
            return " ";
        }
        if (c < 0x20 || c == 0x7F)
        {
            return "";      // \r and other control characters
        }
        static char one[2];
        one[0] = (char)c;
        return one;
    }

    // A character of several bytes: work out which one it is.
    size_t n = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
    uint32_t code = n == 4 ? c & 0x07 : n == 3 ? c & 0x0F : c & 0x1F;
    for (size_t i = 1; i < n; i++)
    {
        if (*at + i >= len || (s[*at + i] & 0xC0) != 0x80)
        {
            n = i;          // broken: skip what's there
            code = 0;
            break;
        }
        code = (code << 6) | (s[*at + i] & 0x3F);
    }
    *at += n;
    switch (code)
    {
    case 0x00A0: return " ";                    // no-break space
    case 0x2018: case 0x2019: case 0x201B: case 0x2032: return "'";
    case 0x201C: case 0x201D: case 0x201F: case 0x2033: return "\"";
    case 0x2010: case 0x2011: case 0x2012: case 0x2013: case 0x2014: case 0x2212: return "-";
    case 0x2026: return "...";
    case 0x2022: case 0x00B7: return "*";
    case 0x00D7: return "x";
    case 0xFE0F: case 0x200D: return "";        // the invisible parts of emoji
    default: return "?";
    }
}

void message_set(const char *new_text, size_t len, bool append)
{
    char clean[MESSAGE_MAX + 1];
    size_t clean_len = 0;
    for (size_t at = 0; at < len && clean_len < MESSAGE_MAX;)
    {
        const char *a = to_ascii((const unsigned char *)new_text, len, &at);
        for (; *a != '\0' && clean_len < MESSAGE_MAX; a++)
        {
            clean[clean_len++] = *a;
        }
    }
    // No blank lines at the end
    while (clean_len > 0 && (clean[clean_len - 1] == '\n' || clean[clean_len - 1] == ' '))
    {
        clean_len--;
    }

    if (!append || text_len == 0)
    {
        text_len = 0;
    }
    else if (clean_len > 0)
    {
        // Drop whole lines from the start until it fits
        while (text_len + 1 + clean_len > MESSAGE_MAX)
        {
            char *nl = memchr(text, '\n', text_len);
            size_t drop = nl != NULL ? (size_t)(nl - text) + 1 : text_len;
            memmove(text, text + drop, text_len - drop);
            text_len -= drop;
        }
        if (text_len > 0)
        {
            text[text_len++] = '\n';
        }
    }
    memcpy(text + text_len, clean, clean_len);
    text_len += clean_len;
    text[text_len] = '\0';
    revision++;
}

void message_clear(void)
{
    text_len = 0;
    text[0] = '\0';
    revision++;
}

const char *message_text(void)
{
    return text;
}

uint32_t message_revision(void)
{
    return revision;
}

static int scale;

void message_set_scale(int new_scale)
{
    scale = new_scale;
}

int message_scale(void)
{
    return scale;
}
