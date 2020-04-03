/* Copyright (c) 2020 Dennis Wölfing
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* sh/interactive.c
 * Interactive input.
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "builtins.h"
#include "interactive.h"
#include "sh.h"

#undef CTRL
#define CTRL(c) ((c) & 0x1F)
#define min(x, y) ((x) < (y) ? (x) : (y))

// TODO: Handle SIGWINCH to detect terminal size changes.

static char* buffer;
static size_t bufferSize;
static size_t bufferUsed;
static size_t position;
static size_t prefixLength;
static char sequenceParam;
static struct winsize winsize;

static inline size_t positionInLine(void) {
    return (prefixLength + position) % winsize.ws_col;
}

static void addToBuffer(char c);
static void delete(bool backspace);

void freeInteractive(void) {
    free(buffer);
}

void initializeInteractive(void) {
    buffer = malloc(80);
    if (!buffer) err(1, "malloc");
    bufferSize = 80;
}

ssize_t readCommandInteractive(char** str, bool newCommand) {
    struct termios oldTermios;
    tcgetattr(0, &oldTermios);
    struct termios termios = oldTermios;
    termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSAFLUSH, &termios);

    tcgetwinsize(2, &winsize);
    prefixLength = printPrompt(newCommand);

    enum {
        NORMAL,
        ESCAPED,
        SEQUENCE
    } state = NORMAL;

    position = 0;
    bufferUsed = 0;

    while (true) {
        char c;
        ssize_t bytesRead = read(0, &c, 1);
        if (bytesRead <= 0) break;
        if (state == NORMAL) {
            if (c == '\e') {
                state = ESCAPED;
            } else if (c == '\b' || c == 0x7F) {
                delete(true);
            } else if (c == '\n') {
                position = bufferUsed;
                addToBuffer('\n');
                break;
            } else if (c == CTRL('D')) {
                if (bufferUsed == 0) break;
            } else {
                addToBuffer(c);
            }
        } else if (state == ESCAPED) {
            if (c == '[') {
                state = SEQUENCE;
                sequenceParam = '\0';
            } else {
                state = NORMAL;
            }
        } else {
            if (c == 'C') {
                if (position < bufferUsed) {
                    if (positionInLine() + 1 < winsize.ws_col) {
                        fputs("\e[C", stderr);
                    } else {
                        fputc('\n', stderr);
                    }
                    position++;
                }
                state = NORMAL;
            } else if (c == 'D') {
                if (position > 0) {
                    if (positionInLine() > 0) {
                        fputs("\e[D", stderr);
                    } else {
                        fprintf(stderr, "\e[A\e[%uG", winsize.ws_col);
                    }
                    position--;
                }
                state = NORMAL;
            } else if (c == '~') {
                if (sequenceParam == '3') {
                    delete(false);
                }
                state = NORMAL;
            } else {
                if (0x40 <= c && c <= 0x7E) {
                    state = NORMAL;
                } else {
                    sequenceParam = c;
                }
            }
        }
    }

    if (bufferUsed == 0 && newCommand) {
        endOfFileReached = true;
    }

    buffer[bufferUsed] = '\0';
    *str = buffer;

    tcsetattr(0, TCSAFLUSH, &oldTermios);
    return bufferUsed;
}

static void addToBuffer(char c) {
    if (bufferUsed + 1 >= bufferSize) {
        void* newBuffer = reallocarray(buffer, bufferSize, 2);
        if (!newBuffer) err(1, "realloc");
        buffer = newBuffer;
        bufferSize *= 2;
    }
    if (position != bufferUsed) {
        memmove(&buffer[position + 1], &buffer[position],
                bufferUsed - position);
    }
    buffer[position++] = c;
    bufferUsed++;
    fputc(c, stderr);
    if (positionInLine() == 0) {
        fputc('\n', stderr);
    }

    if (position != bufferUsed) {
        size_t newlines = 0;
        fwrite(buffer + position, 1, min(winsize.ws_col - positionInLine(),
                bufferUsed - position), stderr);
        for (size_t i = position + winsize.ws_col - positionInLine();
                i <= bufferUsed; i += winsize.ws_col) {
            fputc('\n', stderr);
            newlines++;
            fwrite(buffer + i, 1, min(winsize.ws_col, bufferUsed - i), stderr);
        }
        if (newlines) fprintf(stderr, "\e[%zuA", newlines);
        fprintf(stderr, "\e[%zuG", positionInLine() + 1);
    }
}

static void delete(bool backspace) {
    if (backspace && position == 0) return;
    if (!backspace && position == bufferUsed) return;

    if (backspace && positionInLine() > 0) {
        fputs("\e[D", stderr);
    } else if (backspace) {
        fprintf(stderr, "\e[A\e[%uG", winsize.ws_col);
    }
    fputs("\e[s\e[K", stderr);

    if (backspace) {
        memmove(&buffer[position - 1], &buffer[position],
                bufferUsed - position);
        position--;
    } else {
        memmove(&buffer[position], &buffer[position + 1],
                bufferUsed - position - 1);
    }
    bufferUsed--;
    fwrite(buffer + position, 1, min(winsize.ws_col - positionInLine(),
            bufferUsed - position), stderr);
    for (size_t i = position + winsize.ws_col - positionInLine();
            i <= bufferUsed; i += winsize.ws_col) {
        fputs("\n\e[K", stderr);
        fwrite(buffer + i, 1, min(winsize.ws_col, bufferUsed - i), stderr);
    }
    fputs("\e[u", stderr);
}
