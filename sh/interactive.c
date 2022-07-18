/* Copyright (c) 2020, 2021, 2022 Dennis Wölfing
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

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>

#include "builtins.h"
#include "execute.h"
#include "interactive.h"
#include "sh.h"
#include "variables.h"

#undef CTRL
#define CTRL(c) ((c) & 0x1F)
#define min(x, y) ((x) < (y) ? (x) : (y))

// TODO: Handle SIGWINCH to detect terminal size changes.

struct HistoryEntry {
    char* buffer;
    size_t bufferSize;
    size_t bufferUsed; // not including newline
};

static struct termios currentTermios;
static struct HistoryEntry* history;
static size_t historySize;
static size_t position;
static size_t promptLength;
static char sequenceParam;
static struct winsize winsize;

static inline size_t currentLine(void) {
    return (promptLength + position) / winsize.ws_col;
}

static inline size_t positionInLine(void) {
    return (promptLength + position) % winsize.ws_col;
}

static void addHistoryEntry(struct HistoryEntry entry);
static void addToBuffer(struct HistoryEntry* entry, char c);
static void beginEdit(struct HistoryEntry* old, struct HistoryEntry* new);
static void delete(struct HistoryEntry* entry, bool backspace);
static size_t getCompletions(const char* text, char*** result,
        size_t* completionStart);
static void recallHistoryEntry(struct HistoryEntry* entry);
static bool searchDir(const char* dirname, const char* prefix,
        size_t prefixLength, char*** completions, size_t* numCompletions,
        bool wantDirectory, bool wantExecutable);
static void tab(struct HistoryEntry* entry, bool newCommand);

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

    if (!(lastStatus > 128)) {
        tcgetattr(0, &currentTermios);
    }
    struct termios termios = currentTermios;
    termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &termios);

    tcgetwinsize(2, &winsize);
    promptLength = printPrompt(newCommand);

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
                entry->buffer[entry->bufferUsed] = '\n';
                entry->buffer[entry->bufferUsed + 1] = '\0';
                size_t lines = (promptLength + entry->bufferUsed) /
                        winsize.ws_col;
                if (lines > currentLine()) {
                    fprintf(stderr, "\e[%zuB", lines - currentLine());
                }
                fputc('\n', stderr);
                break;
            } else if (c == '\t') {
                tab(entry, newCommand);
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

    tcsetattr(0, TCSANOW, &currentTermios);
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

static bool isSeparatorForCompletion(unsigned char c) {
    return isblank(c) || c == ';' || c == '&' || c == '|' || c == '<' ||
            c == '>' || c == '$';
}

static size_t getCompletions(const char* text, char*** result,
        size_t* completionStart) {
    // First try to find out what we have to complete. We do not parse the whole
    // command but just try to give useful completions for common cases.

    size_t prefixStart = position;
    while (prefixStart > 0 && !isSeparatorForCompletion(text[prefixStart - 1])
            && text[prefixStart - 1] != '/') {
        prefixStart--;
    }
    size_t wordStart = prefixStart;
    while (wordStart > 0 && !isSeparatorForCompletion(text[wordStart - 1])) {
        wordStart--;
    }

    enum {
        COMPLETION_COMMAND,
        COMPLETION_FILE,
        COMPLETION_VARIABLE,
        COMPLETION_EXECUTABLE,
        COMPLETION_DIRECTORY,
    };
    int completionType;

    if (prefixStart == 0) {
        completionType = COMPLETION_COMMAND;
    } else if (wordStart == 0) {
        completionType = COMPLETION_EXECUTABLE;
    } else if (text[prefixStart - 1] == '$') {
        completionType = COMPLETION_VARIABLE;
        if (position > prefixStart && text[prefixStart] == '{') {
            prefixStart++;
        }
    } else {
        size_t i;
        char sep = '\0';
        for (i = wordStart - 1; i > 0; i--) {
            if (!isblank((unsigned char) text[i])) {
                sep = text[i];
                break;
            }
        }
        if (sep == ';' || sep == '&' || sep == '|' || sep == '\0') {
            if (wordStart == prefixStart) {
                completionType = COMPLETION_COMMAND;
            } else {
                completionType = COMPLETION_EXECUTABLE;
            }
        } else if (sep == '<' || sep == '>') {
            completionType = COMPLETION_FILE;
        } else if (i >= 1 && sep == 'd' && text[i - 1] == 'c' &&
                (i == 1 || isSeparatorForCompletion(text[i - 2]))) {
            completionType = COMPLETION_DIRECTORY;
        } else {
            completionType = COMPLETION_FILE;
        }
    }

    const char* prefix = text + prefixStart;
    size_t prefixLength = position - prefixStart;

    char** completions = NULL;
    size_t completionsUsed = 0;

    // Now that we know what to complete find all the possible completions.
    if (completionType == COMPLETION_COMMAND) {
        for (const struct builtin* builtin = builtins; builtin->name;
                builtin++) {
            if (strncmp(prefix, builtin->name, prefixLength) == 0) {
                char* name = strdup(builtin->name);
                if (!name) {
                    goto fail;
                }
                addToArray((void**) &completions, &completionsUsed, &name,
                        sizeof(char*));
            }
        }

        for (size_t i = 0; i < numFunctions; i++) {
            if (strncmp(prefix, functions[i]->name, prefixLength) == 0) {
                char* name = strdup(functions[i]->name);
                if (!name) goto fail;
                addToArray((void**) &completions, &completionsUsed, &name,
                        sizeof(char*));
            }
        }

        const char* path = getVariable("PATH");
        if (path) {
            while (true) {
                size_t length = strcspn(path, ":");
                char* dirname = length ? strndup(path, length) : strdup(".");
                if (!dirname) {
                    goto fail;
                }
                if (!searchDir(dirname, prefix, prefixLength, &completions,
                        &completionsUsed, false, true)) {
                    free(dirname);
                    goto fail;
                }
                free(dirname);

                if (!path[length]) break;
                path += length + 1;
            }
        }
    } else if (completionType == COMPLETION_FILE ||
            completionType == COMPLETION_DIRECTORY ||
            completionType == COMPLETION_EXECUTABLE) {
        bool directory = completionType == COMPLETION_DIRECTORY;
        bool executable = completionType == COMPLETION_EXECUTABLE;

        char* dirname;
        if (wordStart != prefixStart) {
            dirname = strndup(text + wordStart, prefixStart - wordStart);
        } else {
            dirname = strdup(".");
        }
        if (!dirname) goto fail;

        if (!searchDir(dirname, prefix, prefixLength, &completions,
                &completionsUsed, directory, executable)) {
            free(dirname);
            goto fail;
        }
        free(dirname);
    } else if (completionType == COMPLETION_VARIABLE) {
        for (size_t i = 0; i < variablesAllocated; i++) {
            struct ShellVar* var = &variables[i];
            if (strncmp(prefix, var->name, prefixLength) == 0) {
                char* name = strdup(var->name);
                if (!name) goto fail;
                addToArray((void**) &completions, &completionsUsed, &name,
                        sizeof(char*));
            }
        }
    }

    *completionStart = prefixStart;
    *result = completions;
    return completionsUsed;

fail:
    for (size_t i = 0; i < completionsUsed; i++) {
        free(completions[i]);
    }
    free(completions);
    return 0;
}

static void recallHistoryEntry(struct HistoryEntry* entry) {
    size_t linesUp = currentLine();
    if (linesUp > 0) {
        fprintf(stderr, "\e[%zuA", linesUp);
    }
    fprintf(stderr, "\e[%zuG\e[J", promptLength + 1);

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

static bool searchDir(const char* dirname, const char* prefix,
        size_t prefixLength, char*** completions, size_t* numCompletions,
        bool wantDirectory, bool wantExecutable) {
    DIR* dir = opendir(dirname);
    if (!dir) return true;

    errno = 0;
    struct dirent* dirent = readdir(dir);
    while (dirent) {
        if (strncmp(prefix, dirent->d_name, prefixLength) != 0) goto next;
        if (prefixLength == 0 && (strcmp(dirent->d_name, ".") == 0 ||
                strcmp(dirent->d_name, "..") == 0)) {
            goto next;
        }

        bool isDirectory = dirent->d_type == DT_DIR;
        if (dirent->d_type == DT_UNKNOWN || wantExecutable) {
            struct stat st;
            if (fstatat(dirfd(dir), dirent->d_name, &st, 0) < 0) {
                goto next;
            }

            isDirectory = S_ISDIR(st.st_mode);
            bool isExecutable = st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH);
            if (wantExecutable && !isExecutable) goto next;
        }

        if (wantDirectory && !isDirectory) goto next;

        char* name = malloc(strlen(dirent->d_name) + isDirectory + 1);
        if (!name) return false;
        strcpy(name, dirent->d_name);
        if (isDirectory) {
            strcat(name, "/");
        }
        addToArray((void**) completions, numCompletions, &name, sizeof(char*));

next:
        dirent = readdir(dir);
    }
    closedir(dir);
    return true;
}

static int sortString(const void* a, const void* b) {
    return strcmp(*(const char**) a, *(const char**) b);
}

static void tab(struct HistoryEntry* entry, bool newCommand) {
    if (entry->bufferUsed == 0) return;

    char** completions;
    size_t completionStart;
    size_t numCompletions = getCompletions(entry->buffer, &completions,
            &completionStart);
    if (numCompletions == 0) return;

    if (numCompletions == 1) {
        // Only a single completion exists.
        char* completion = completions[0] + (position - completionStart);
        char lastCharacter = '\0';
        while (*completion) {
            addToBuffer(entry, *completion);
            lastCharacter = *completion;
            completion++;
        }
        if (lastCharacter != '/') {
            addToBuffer(entry, ' ');
        }
        free(completions[0]);
        free(completions);
        return;
    }

    qsort(completions, numCompletions, sizeof(completions[0]), sortString);

    // Check whether all completions share a common prefix. The list is sorted
    // so it is enough to just look at the first and last completions.
    size_t commonLength = 0;
    char* first = completions[0];
    char* last = completions[numCompletions - 1];
    while (first[commonLength] == last[commonLength]) {
        commonLength++;
    }

    if (commonLength > position - completionStart) {
        for (size_t i = position - completionStart; i < commonLength; i++) {
            addToBuffer(entry, first[i]);
        }
        for (size_t i = 0; i < numCompletions; i++) {
            free(completions[i]);
        }
        free(completions);
        return;
    }

    size_t lines = (promptLength + entry->bufferUsed) / winsize.ws_col;
    if (lines > currentLine()) {
        fprintf(stderr, "\e[%zuB", lines - currentLine());
    }
    fputc('\n', stderr);
    for (size_t i = 0; i < numCompletions; i++) {
        fputs(completions[i], stderr);
        fputc(i == numCompletions - 1 ? '\n' : ' ', stderr);
    }

    printPrompt(newCommand);
    size_t pos = position;
    size_t posInLine = positionInLine();
    size_t line = currentLine();
    position = 0;
    recallHistoryEntry(entry);

    // Move the cursor to the position in the command where it was before.
    size_t newLine = currentLine();
    if (line < newLine) {
        fprintf(stderr, "\e[%zuA", newLine - line);
    }
    if (posInLine < positionInLine()) {
        fprintf(stderr, "\e[%zuD", positionInLine() - posInLine);
    } else if (posInLine > positionInLine()) {
        fprintf(stderr, "\e[%zuC", posInLine - positionInLine());
    }
    position = pos;

    for (size_t i = 0; i < numCompletions; i++) {
        free(completions[i]);
    }
    free(completions);
}
