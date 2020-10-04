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

struct HistoryEntry {
    char* buffer;
    size_t bufferSize;
    size_t bufferUsed; // not including newline
};

static struct HistoryEntry* history;
static size_t historySize;
static size_t position;
static size_t prefixLength;
static char sequenceParam;
static struct winsize winsize;

static inline size_t positionInLine(void) {
    return (prefixLength + position) % winsize.ws_col;
}

static void addHistoryEntry(struct HistoryEntry entry);
static void addToBuffer(struct HistoryEntry* entry, char c);
static void beginEdit(struct HistoryEntry* old, struct HistoryEntry* new);
static void delete(struct HistoryEntry* entry, bool backspace);
static void recallHistoryEntry(struct HistoryEntry* entry);

void freeInteractive(void) {
    for (size_t i = 0; i < historySize; i++) {
        free(history[i].buffer);
    }
    free(history);
}

void initializeInteractive(void) {
    history = NULL;
    historySize = 0;
}

void readCommandInteractive(const char** str, bool newCommand) {
    struct HistoryEntry newEntry;
    newEntry.buffer = malloc(80);
    if (!newEntry.buffer) err(1, "malloc");
    newEntry.bufferSize = 80;
    newEntry.bufferUsed = 0;
    struct HistoryEntry editedEntry = {0};
    struct HistoryEntry* entry = &newEntry;
    size_t historyPosition = historySize;
    bool historyEdited = false;

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

    while (true) {
        char c;
        ssize_t bytesRead = read(0, &c, 1);
        if (bytesRead <= 0) break;
        if (state == NORMAL) {
            if (c == '\e') {
                state = ESCAPED;
            } else if (c == '\b' || c == 0x7F) {
                if (entry != &newEntry && !historyEdited) {
                    beginEdit(entry, &editedEntry);
                    historyEdited = true;
                    entry = &editedEntry;
                }
                delete(entry, true);
            } else if (c == '\n') {
                position = entry->bufferUsed;
                entry->buffer[entry->bufferUsed] = '\n';
                entry->buffer[entry->bufferUsed + 1] = '\0';
                fputc('\n', stderr);
                break;
            } else if (c == CTRL('D')) {
                if (entry->bufferUsed == 0) {
                    entry->buffer[0] = '\0';
                    if (newCommand) {
                        endOfFileReached = true;
                    }
                    break;
                }
            } else {
                if (entry != &newEntry && !historyEdited) {
                    beginEdit(entry, &editedEntry);
                    historyEdited = true;
                    entry = &editedEntry;
                }
                addToBuffer(entry, c);
            }
        } else if (state == ESCAPED) {
            if (c == '[') {
                state = SEQUENCE;
                sequenceParam = '\0';
            } else {
                state = NORMAL;
            }
        } else {
            if (c == 'A') {
                if (historyPosition > 0) {
                    if (historyEdited) {
                        free(history[historyPosition].buffer);
                        history[historyPosition] = editedEntry;
                        historyEdited = false;
                    }
                    historyPosition--;
                    entry = history + historyPosition;
                    recallHistoryEntry(entry);
                }
                state = NORMAL;
            } else if (c == 'B') {
                if (historyPosition + 1 <= historySize) {
                    if (historyEdited) {
                        free(history[historyPosition].buffer);
                        history[historyPosition] = editedEntry;
                        historyEdited = false;
                    }
                    historyPosition++;
                    entry = historyPosition == historySize ? &newEntry :
                            history + historyPosition;
                    recallHistoryEntry(entry);
                }
                state = NORMAL;
            } else if (c == 'C') {
                if (position < entry->bufferUsed) {
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
                    if (entry != &newEntry && !historyEdited) {
                        beginEdit(entry, &editedEntry);
                        historyEdited = true;
                        entry = &editedEntry;
                    }
                    delete(entry, false);
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

    bool addToHistory = entry->bufferUsed != 0;
    if (historySize > 0) {
        struct HistoryEntry* lastEntry = &history[historySize - 1];
        if (entry->bufferUsed == lastEntry->bufferUsed &&
                memcmp(entry->buffer, lastEntry->buffer,
                entry->bufferUsed) == 0) {
            entry = lastEntry;
            addToHistory = false;
        }
    }

    if (entry->bufferUsed == 0) {
        *str = entry->buffer[0] == '\n' ? "\n" : "";
        free(newEntry.buffer);
    } else {
        *str = entry->buffer;
        if (entry != &newEntry) {
            free(newEntry.buffer);
        }
    }

    bool recalled = entry != &newEntry && entry != &editedEntry;
    if (recalled && addToHistory) {
        newEntry.buffer = malloc(entry->bufferUsed + 2);
        if (!newEntry.buffer) err(1, "malloc");
        memcpy(newEntry.buffer, entry->buffer, entry->bufferUsed + 2);
        newEntry.bufferUsed = entry->bufferUsed;
        newEntry.bufferSize = entry->bufferUsed + 2;
        entry = &newEntry;
    }

    if (addToHistory) {
        addHistoryEntry(*entry);
    }

    tcsetattr(0, TCSAFLUSH, &oldTermios);
}

static void addHistoryEntry(struct HistoryEntry entry) {
    void* newHistory = reallocarray(history, historySize + 1,
            sizeof(struct HistoryEntry));
    if (!newHistory) err(1, "malloc");
    history = newHistory;
    historySize++;
    history[historySize - 1] = entry;
}

static void addToBuffer(struct HistoryEntry* entry, char c) {
    if (entry->bufferUsed + 2 >= entry->bufferSize) {
        void* newBuffer = reallocarray(entry->buffer, entry->bufferSize, 2);
        if (!newBuffer) err(1, "realloc");
        entry->buffer = newBuffer;
        entry->bufferSize *= 2;
    }
    if (position != entry->bufferUsed) {
        memmove(&entry->buffer[position + 1], &entry->buffer[position],
                entry->bufferUsed - position);
    }
    entry->buffer[position++] = c;
    entry->bufferUsed++;
    fputc(c, stderr);
    if (positionInLine() == 0) {
        fputc('\n', stderr);
    }

    if (position != entry->bufferUsed) {
        size_t newlines = 0;
        fwrite(entry->buffer + position, 1,
                min(winsize.ws_col - positionInLine(),
                entry->bufferUsed - position), stderr);
        for (size_t i = position + winsize.ws_col - positionInLine();
                i <= entry->bufferUsed; i += winsize.ws_col) {
            fputc('\n', stderr);
            newlines++;
            fwrite(entry->buffer + i, 1, min(winsize.ws_col,
                    entry->bufferUsed - i), stderr);
        }
        if (newlines) fprintf(stderr, "\e[%zuA", newlines);
        fprintf(stderr, "\e[%zuG", positionInLine() + 1);
    }
}

static void beginEdit(struct HistoryEntry* old, struct HistoryEntry* new) {
    void* newBuffer = malloc(old->bufferUsed + 2);
    if (!newBuffer) err(1, "malloc");
    memcpy(newBuffer, old->buffer, old->bufferUsed);
    new->buffer = old->buffer;
    new->bufferSize = old->bufferSize;
    new->bufferUsed = old->bufferUsed;
    old->buffer = newBuffer;
    old->bufferSize = old->bufferUsed + 2;
}

static void delete(struct HistoryEntry* entry, bool backspace) {
    if (backspace && position == 0) return;
    if (!backspace && position == entry->bufferUsed) return;

    if (backspace && positionInLine() > 0) {
        fputs("\e[D", stderr);
    } else if (backspace) {
        fprintf(stderr, "\e[A\e[%uG", winsize.ws_col);
    }
    fputs("\e[s\e[K", stderr);

    if (backspace) {
        memmove(&entry->buffer[position - 1], &entry->buffer[position],
                entry->bufferUsed - position);
        position--;
    } else {
        memmove(&entry->buffer[position], &entry->buffer[position + 1],
                entry->bufferUsed - position - 1);
    }
    entry->bufferUsed--;
    fwrite(entry->buffer + position, 1, min(winsize.ws_col - positionInLine(),
            entry->bufferUsed - position), stderr);
    for (size_t i = position + winsize.ws_col - positionInLine();
            i <= entry->bufferUsed; i += winsize.ws_col) {
        fputs("\n\e[K", stderr);
        fwrite(entry->buffer + i, 1, min(winsize.ws_col, entry->bufferUsed - i),
                stderr);
    }
    fputs("\e[u", stderr);
}

static void recallHistoryEntry(struct HistoryEntry* entry) {
    size_t linesUp = (prefixLength + position) / winsize.ws_col;
    if (linesUp > 0) {
        fprintf(stderr, "\e[%zuA", linesUp);
    }
    fprintf(stderr, "\e[%zuG\e[J", prefixLength + 1);

    position = 0;
    fwrite(entry->buffer, 1, min(winsize.ws_col - positionInLine(),
            entry->bufferUsed), stderr);
    for (size_t i = winsize.ws_col - positionInLine(); i <= entry->bufferUsed;
            i += winsize.ws_col) {
        fputc('\n', stderr);
        fwrite(entry->buffer + i, 1, min(winsize.ws_col, entry->bufferUsed - i),
                stderr);
    }
    position = entry->bufferUsed;
}
