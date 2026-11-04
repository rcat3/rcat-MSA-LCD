#ifndef RCAT_MESSAGE_H
#define RCAT_MESSAGE_H

/*
 * The text the Terminal feature shows, typed in on the phone remote. It's
 * kept as plain ASCII, which is all the terminal font has: anything else is
 * changed to the nearest ASCII (curly quotes to straight ones, dashes to
 * hyphens) or to '?'.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MESSAGE_MAX 600     // characters kept; adding past this drops the oldest lines

/* Replace the text, or add it on a new line after what's there. text is
 * UTF-8 and needn't end with a zero. */
void message_set(const char *text, size_t len, bool append);
void message_clear(void);

const char *message_text(void);

/* Goes up by one with every change, so a page can tell when to redraw. */
uint32_t message_revision(void);

/* The letter size the Terminal is showing it at, 1 to 6 (8 to 48 px), for
 * the phone remote to show. The Terminal sets it. */
void message_set_scale(int scale);
int message_scale(void);

#endif
