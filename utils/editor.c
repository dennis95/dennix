/* Copyright (c) 2017, 2018, 2019, 2020 Dennis Wölfing
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

/* utils/editor.c
 * Text editor.
 */

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>

// The code assumes that wchar_t has a UTF-32 encoding so that we can save
// invalid characters in the range 0xD800 to 0xD8FF.
static_assert(L'ä' == 0xE4, "wchar_t is not UTF-32 encoded.");
static_assert(L'☺' == 0x263A, "wchar_t is not UTF-32 encoded.");
static_assert(L'𝜙' == 0x1D719, "wchar_t is not UTF-32 encoded.");

struct line {
    wchar_t* buffer;
    size_t length;
    size_t bufferSize;
};

#undef CTRL
#define CTRL(c) ((c) & 0x1F)

static size_t height = 25;
static size_t width = 80;

static size_t cursorX;
static size_t cursorY;
static size_t linePos;
static size_t logicalX;

static const char* filename;

static struct line* lines;
static size_t linesAllocated;
static size_t linesUsed;

static struct termios oldTermios;

static size_t windowX;
static size_t windowY;

static const unsigned int tabsize = 8;

static struct line* addLine(size_t lineNumber);
static void backspace(void);
static void delete(void);
static void drawLine(size_t y);
static void drawLines(void);
static noreturn void error(const char* msg, ...);
static void getInput(void);
static void handleKey(char c);
static void handleSequence(char c);
static void newline(void);
static void putCharacter(wchar_t wc);
static void readFile(const char* filename);
static void removeAt(size_t y, size_t position);
static void restoreTermios(void);
static void saveFile(const char* filename);
static void updateCursorPosition(void);
static bool updateLinePos(void);
static void updateLogicalPos(void);

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "");

    if (argc >= 2) {
        filename = argv[1];
    } else {
        // The editor currently cannot ask for a filename when saving so we
        // already need to know one at startup.
        errx(1, "No filename given");
    }

    tcgetattr(0, &oldTermios);
    atexit(restoreTermios);
    struct termios newTermios = oldTermios;
#ifdef IXON
    newTermios.c_iflag &= ~IXON;
#endif
    newTermios.c_lflag &= ~(ECHO | ICANON);
    newTermios.c_cc[VMIN] = 0;
    tcsetattr(0, TCSAFLUSH, &newTermios);

    struct winsize ws;
    if (tcgetwinsize(1, &ws) == 0) {
        height = ws.ws_row;
        width = ws.ws_col;
    }

    setbuf(stdout, NULL);

    readFile(filename);
    fputs("\e[?1049h", stdout);
    drawLines();
    updateCursorPosition();
    while (true) {
        getInput();
    }
}

static struct line* addLine(size_t lineNumber) {
    if (linesUsed == linesAllocated) {
        if (linesAllocated == 0) {
            linesAllocated = 16;
        }
        lines = reallocarray(lines, linesAllocated, 2 * sizeof(struct line));
        if (!lines) error("reallocarray");
        linesAllocated *= 2;
    }

    memmove(&lines[lineNumber + 1], &lines[lineNumber],
            (linesUsed - lineNumber) * sizeof(struct line));

    struct line* newLine = &lines[lineNumber];
    newLine->buffer = malloc(80 * sizeof(wchar_t));
    if (!newLine->buffer) error("malloc");
    newLine->bufferSize = 80;
    newLine->length = 0;

    return newLine;
}

static void backspace(void) {
    if (linePos > 0) {
        removeAt(cursorY + windowY, linePos - 1);
        linePos--;
        if (updateLinePos()) {
            drawLines();
        } else {
            drawLine(cursorY);
        }
        logicalX = cursorX + windowX;
    } else if (cursorY + windowY > 0) {
        size_t y = cursorY + windowY - 1;
        size_t length = lines[y].length;
        removeAt(y, length);

        linePos = length;

        if (cursorY > 0) {
            cursorY--;
        } else {
            windowY--;
        }
        updateLinePos();
        logicalX = cursorX + windowX;
        drawLines();
    }
    updateCursorPosition();
}

static void delete(void) {
    size_t y = cursorY + windowY;
    if (linePos >= lines[y].length && y >= linesUsed - 1) return;

    removeAt(y, linePos);
    drawLines();
    updateCursorPosition();
}

static void drawLine(size_t y) {
    printf("\e[%zuH\e[2K", y + 1);
    if (y + windowY >= linesUsed) return;

    struct line line = lines[y + windowY];
    mbstate_t ps = {0};

    size_t i = 0;
    size_t x = 0;
    while (i < line.length && x < windowX + width) {
        wchar_t wc = line.buffer[i];
        if (wc >= 0xD800 && wc <= 0xD8FF) {
            wc = L'�';
        }

        if (wc == L'\t') {
            x += tabsize - x % tabsize;
            if (x >= windowX) {
                printf("\e[%zuG", x - windowX + 1);
            }
        } else if (x >= windowX) {
            char buffer[MB_CUR_MAX];
            size_t size = wcrtomb(buffer, wc, &ps);
            fwrite(buffer, 1, size, stdout);
            x++;
        } else {
            x++;
        }

        i++;
    }
}

static void drawLines(void) {
    for (size_t y = 0; y < height; y++) {
        drawLine(y);
    }
}

static noreturn void error(const char* msg, ...) {
    fputs("\e[?1049l", stdout);
    va_list ap;
    va_start(ap, msg);
    verr(1, msg, ap);
}

static enum {
    NORMAL,
    ESCAPED,
    SEQUENCE
} state = NORMAL;

static char sequenceParam;

static void getInput(void) {
    char c;
    if (read(0, &c, 1) == 1) {
        if (state == NORMAL) {
            handleKey(c);
        } else if (state == ESCAPED) {
            if (c == '[') {
                sequenceParam = '\0';
                state = SEQUENCE;
            } else {
                state = NORMAL;
                handleKey(c);
            }
        } else if (state == SEQUENCE) {
            handleSequence(c);
        }
    }
}

static void handleKey(char c) {
    static mbstate_t ps = {0};
    wchar_t wc;

    size_t result = mbrtowc(&wc, &c, 1, &ps);
    if (result == (size_t) -2) {
        return;
    } else if (result == (size_t) -1) {
        memset(&ps, 0, sizeof(ps));
        return;
    }

    if (c == '\e') {
        state = ESCAPED;
    } else if (c == CTRL('Q')) {
        fputs("\e[?1049l", stdout);
        exit(0);
    } else if (c == CTRL('S')) {
        saveFile(filename);
    } else if (c == '\b' || c == 0x7F) {
        backspace();
    } else if (c == '\n') {
        newline();
    } else if (CTRL(c) != c || c == '\t') {
        putCharacter(wc);
    }
}

static void handleSequence(char c) {
    switch (c) {
    case 'A':
        if (cursorY > 0) {
            cursorY--;
        } else if (windowY > 0) {
            windowY--;
        }

        updateLogicalPos();
        if (updateLinePos() || cursorY == 0) {
            drawLines();
        }
        updateCursorPosition();
        break;
    case 'B':
        if (cursorY < height - 1 && windowY + cursorY + 1 < linesUsed) {
            cursorY++;
        } else if (windowY + cursorY + 1 < linesUsed) {
            windowY++;
        }

        updateLogicalPos();
        if (updateLinePos() || cursorY == height - 1) {
            drawLines();
        }
        updateCursorPosition();
        break;
    case 'C':
        linePos++;

        if (updateLinePos()) {
            drawLines();
        }
        logicalX = cursorX + windowX;
        updateCursorPosition();
        break;
    case 'D':
        if (linePos > 0) {
            linePos--;
        }

        if (updateLinePos()) {
            drawLines();
        }
        logicalX = cursorX + windowX;
        updateCursorPosition();
        break;
    case '~':
        if (sequenceParam == '3') {
            delete();
            logicalX = cursorX + windowX;
        }
        break;
    default:
        if (0x40 <= c && c <= 0x7E) {
            // Unsupported escape sequence
        } else {
            sequenceParam = c;
            return;
        }
    }

    state = NORMAL;
}

static void newline(void) {
    struct line* newLine = addLine(cursorY + windowY + 1);
    linesUsed++;

    struct line* currentLine = newLine - 1;
    if (newLine->bufferSize < currentLine->length - linePos) {
        newLine->buffer = reallocarray(newLine->buffer,
                currentLine->length - linePos, sizeof(wchar_t));
        if (!newLine->buffer) error("reallocarray");
        newLine->bufferSize = currentLine->length - linePos;
    }

    memcpy(newLine->buffer, currentLine->buffer + linePos,
            (currentLine->length - linePos) * sizeof(wchar_t));
    newLine->length = currentLine->length - linePos;
    currentLine->length = linePos;

    if (cursorY < height - 1) {
        cursorY++;
    } else {
        windowY++;
    }

    linePos = 0;
    logicalX = 0;
    updateLinePos();
    drawLines();
    updateCursorPosition();
}

static void putCharacter(wchar_t wc) {
    struct line* line = &lines[cursorY + windowY];
    if (line->length + 1 == line->bufferSize) {
        line->buffer = reallocarray(line->buffer,
                line->bufferSize * sizeof(wchar_t), 2);
        if (!line->buffer) error("reallocarray");
        line->bufferSize *= 2;
    }
    memmove(line->buffer + linePos + 1, line->buffer + linePos,
            (line->length - linePos) * sizeof(wchar_t));
    line->buffer[linePos] = wc;
    line->length++;

    linePos++;

    if (updateLinePos()) {
        drawLines();
    } else {
        drawLine(cursorY);
    }
    logicalX = cursorX + windowX;
    updateCursorPosition();
}

static void readFile(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        if (errno != ENOENT) error("'%s'", filename);
        addLine(0);
        linesUsed = 1;
        return;
    }

    char buffer[MB_CUR_MAX + 1];
    size_t bytesInBuffer = 0;
    mbstate_t ps = {0};

    while (!feof(file) || bytesInBuffer > 0) {
        struct line* line = addLine(linesUsed);
        wchar_t wc;

        do {
            bytesInBuffer += fread(buffer + bytesInBuffer, 1,
                    MB_CUR_MAX - bytesInBuffer, file);
            if (bytesInBuffer == 0) break;
            buffer[bytesInBuffer] = '\0';

            const char* s = buffer;
            size_t result = mbsrtowcs(&wc, &s, 1, &ps);

            if (result != 1) {
                // Encode invalid bytes as invalid wchar_t so that encoding
                // errors can be preserved when saving.
                wc = 0xD800 + (unsigned char) buffer[0];
                memset(&ps, 0, sizeof(ps));
                s = buffer + 1;
            }

            if (wc != L'\n') {
                if (line->length >= line->bufferSize) {
                    wchar_t* newBuffer = reallocarray(line->buffer, 2,
                            line->bufferSize * sizeof(wchar_t));
                    if (!newBuffer) error("reallocarray");
                    line->buffer = newBuffer;
                    line->bufferSize *= 2;
                }

                line->buffer[line->length++] = wc;
            }

            if (s) {
                bytesInBuffer -= s - buffer;
                if (bytesInBuffer > 0) {
                    memmove(buffer, s, bytesInBuffer);
                }
            } else {
                bytesInBuffer = 0;
            }
        } while (wc != L'\n');

        if (ferror(file)) error("'%s'", filename);
        linesUsed++;
    }

    fclose(file);

    if (linesUsed == 0) {
        addLine(0);
        linesUsed = 1;
    }
}

static void removeAt(size_t y, size_t position) {
    struct line* line = &lines[y];
    if (position < line->length) {
        memmove(line->buffer + position, line->buffer + position + 1,
                (line->length - position - 1) * sizeof(wchar_t));
        line->length--;
    } else {
        struct line* next = line + 1;
        if (line->bufferSize < line->length + next->length) {
            line->buffer = reallocarray(line->buffer,
                    line->length + next->length, sizeof(wchar_t));
            if (!line->buffer) error("realloc");
            line->bufferSize = line->length + next->length;
        }
        memcpy(line->buffer + line->length, next->buffer,
                next->length * sizeof(wchar_t));

        line->length += next->length;

        free(next->buffer);
        linesUsed--;
        memmove(next, next + 1, (linesUsed - y - 1) * sizeof(struct line));
    }
}

static void restoreTermios(void) {
    tcsetattr(0, TCSAFLUSH, &oldTermios);
}

static void saveFile(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) error("'%s'", filename);

    for (size_t i = 0; i < linesUsed; i++) {
        struct line* line = &lines[i];
        mbstate_t ps = {0};
        for (size_t i = 0; i < line->length; i++) {
            wchar_t wc = line->buffer[i];
            if (wc >= 0xD800 && wc <= 0xD8FF) {
                fputc(wc - 0xD800, file);
            } else {
                char buffer[MB_CUR_MAX];
                size_t size = wcrtomb(buffer, wc, &ps);
                fwrite(buffer, 1, size, file);
            }
        }
        fputc('\n', file);
    }

    if (ferror(file)) {
        int errnum = errno;
        fclose(file);
        errno = errnum;
        error("'%s'", filename);
    }
    if (fclose(file)) {
        error("'%s'", filename);
    }
}

static void updateCursorPosition(void) {
    printf("\e[%zu;%zuH", cursorY + 1, cursorX + 1);
}

static bool updateLinePos(void) {
    // Move the cursor to the current line position.
    struct line* line = &lines[cursorY + windowY];
    if (linePos > line->length) {
        linePos = line->length;
    }

    size_t x = 0;
    for (size_t i = 0; i < linePos; i++) {
        wchar_t wc = line->buffer[i];

        if (wc == L'\t') {
            x += tabsize - x % tabsize;
        } else {
            x++;
        }
    }

    if (x < windowX) {
        windowX = x;
        cursorX = 0;
        return true;
    } else if (x >= windowX + width) {
        windowX = x - width + 1;
        cursorX = width - 1;
        return true;
    }

    cursorX = x - windowX;
    return false;
}

static void updateLogicalPos(void) {
    // Move the line position to the logical x position.
    struct line* line = &lines[cursorY + windowY];

    size_t i = 0;
    size_t x = 0;

    while (x < logicalX && i < line->length) {
        wchar_t wc = line->buffer[i++];

        if (wc == L'\t') {
            x += tabsize - x % tabsize;
        } else {
            x++;
        }
    }

    if (x > logicalX) {
        i--;
    }

    linePos = i;
}
