/* Copyright (c) 2021 Dennis Wölfing
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

/* apps/terminal.c
 * GUI terminal.
 */

#include <dxui.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>
#include <sys/wait.h>
#include <dennix/kbkeys.h>

// Some of this code is shared (with modifications) with kernel/src/console.cpp.
// When making changes to the shared code it is important to keep these two
// files in sync so that both can be equally considered the "dennix" terminal.

typedef struct {
    dxui_color fgColor;
    dxui_color bgColor;
    uint8_t vgaColor;
} Color;

typedef struct {
    unsigned int x;
    unsigned int y;
} CharPos;

typedef struct {
    wchar_t wc;
    dxui_color fg;
    dxui_color bg;
} TextEntry;

static const dxui_color vgaColors[16] = {
    RGB(0, 0, 0),
    RGB(0, 0, 170),
    RGB(0, 170, 0),
    RGB(0, 170, 170),
    RGB(170, 0, 0),
    RGB(170, 0, 170),
    RGB(170, 85, 0),
    RGB(170, 170, 170),
    RGB(85, 85, 85),
    RGB(85, 85, 255),
    RGB(85, 255, 85),
    RGB(85, 255, 255),
    RGB(255, 85, 85),
    RGB(255, 85, 255),
    RGB(255, 255, 85),
    RGB(255, 255, 255),
};

static const Color defaultColor = {
    .fgColor = RGB(170, 170, 170),
    .bgColor = RGB(0, 0, 0),
    .vgaColor = 0x07
};
static const unsigned int tabsize = 8;

static const struct {
    int key;
    const char* sequence;
} sequences[] = {
    { KB_UP, "\e[A" },
    { KB_DOWN, "\e[B" },
    { KB_RIGHT, "\e[C" },
    { KB_LEFT, "\e[D" },
    { KB_END, "\e[F" },
    { KB_HOME, "\e[H" },
    { KB_INSERT, "\e[2~" },
    { KB_DELETE, "\e[3~" },
    { KB_PAGEUP, "\e[5~" },
    { KB_PAGEDOWN, "\e[6~" },
    { KB_F1, "\e[OP" },
    { KB_F2, "\e[OQ" },
    { KB_F3, "\e[OR" },
    { KB_F4, "\e[OS" },
    { KB_F5, "\e[15~" },
    { KB_F6, "\e[17~" },
    { KB_F7, "\e[18~" },
    { KB_F8, "\e[19~" },
    { KB_F9, "\e[20~" },
    { KB_F10, "\e[21~" },
    { KB_F11, "\e[23~" },
    { KB_F12, "\e[24~" },
    { 0, 0 }
};

enum {
    NORMAL,
    ESCAPED,
    CSI,
    OSC,
    OSC_ESCAPED,
};

static volatile sig_atomic_t childExited;
static dxui_context* context;
static dxui_color* lfb;
static bool needsRedraw;
static bool resized;
static int terminalController;
static dxui_window* window;
static dxui_dim windowDim;
static struct winsize windowSize;

static TextEntry* currentBuffer;
static TextEntry* primaryBuffer;
static TextEntry* alternateBuffer;

static Color alternateSavedColor;
static CharPos alternateSavedPos;
static Color color;
static CharPos cursorPos;
static bool cursorVisible = true;
static bool endOfLine;
static bool fgIsVgaColor;
static bool reversedColors;
static Color savedColor;
static CharPos savedPos;

#define MAX_PARAMS 16
static unsigned int params[MAX_PARAMS];
static bool paramSpecified[MAX_PARAMS];
static size_t paramIndex;
static mbstate_t ps;
static bool questionMarkModifier;
static int status;

static void clear(CharPos from, CharPos to, Color color);
static void createTerminal(void);
static void draw(void);
static void drawCell(size_t row, size_t col);
static void ensureStdFdsAreUsed(void);
static void handleResize(void);
static void handleSigchld(int signo);
static void onClose(dxui_window* window);
static void onKey(dxui_control* control, dxui_key_event* event);
static void onResize(dxui_window* window, dxui_resize_event* event);
static void printCharacter(char c);
static void printCharacterRaw(char c);
static void readController(void);
static Color reverse(Color c);
static void scroll(unsigned int lines, Color color, bool up);
static void setGraphicsRendition(void);
static void shutdown(void);
static bool writeAll(int fd, const void* buffer, size_t size);

static inline bool usingAlternateBuffer(void) {
    return currentBuffer == alternateBuffer;
}

static void clear(CharPos from, CharPos to, Color color) {
    size_t bufferStart = from.x + windowSize.ws_col * from.y;
    size_t bufferEnd = to.x + windowSize.ws_col * to.y;
    for (size_t i = bufferStart; i <= bufferEnd; i++) {
        currentBuffer[i].wc = L' ';
        currentBuffer[i].fg = color.fgColor;
        currentBuffer[i].bg = color.bgColor;
    }
}

static void createTerminal(void) {
    ensureStdFdsAreUsed();

    terminalController = posix_openpt(O_RDWR | O_CLOFORK | O_NOCTTY);
    if (terminalController < 0 || grantpt(terminalController) < 0 ||
            unlockpt(terminalController) < 0) {
        dxui_panic(context, "Failed to create pseudo terminal");
    }

    tcgetwinsize(terminalController, &windowSize);
    windowSize.ws_col = 80;
    windowSize.ws_row = 25;
    tcsetwinsize(terminalController, &windowSize);

    const char* name = ptsname(terminalController);
    if (!name) dxui_panic(context, "ptsname");

    int pty = open(name, O_RDWR | O_CLOEXEC | O_NOCTTY);
    if (pty < 0) dxui_panic(context, "Failed to open pseudo terminal");

    pid_t pid = fork();
    if (pid < 0) {
        dxui_panic(context, "fork");
    } else if (pid == 0) {
        pid = setsid();
        if (pid < 0) _Exit(127);

        if (tcsetsid(pty, pid) < 0) _Exit(127);

        close(0);
        close(1);
        close(2);

        if (dup(pty) != 0 || dup(pty) != 1 || dup(pty) != 2) _Exit(127);

        setenv("TERM", "dennix", 1);
        execl("/bin/sh", "sh", NULL);
        _Exit(127);
    }

    close(pty);
}

static void draw(void) {
    for (size_t row = 0; row < windowSize.ws_row; row++) {
        for (size_t col = 0; col < windowSize.ws_col; col++) {
            drawCell(row, col);
        }
    }

    dxui_rect rect;
    rect.pos = (dxui_pos) {0, 0};
    rect.dim = windowDim;
    dxui_update_framebuffer(window, rect);
}

static void drawCell(size_t row, size_t col) {
    TextEntry* entry = &currentBuffer[row * windowSize.ws_col + col];

    dxui_pos pos = { col * 9, row * 16 };
    int width = col == windowSize.ws_col - 1U ? 8 : 9;
    for (int y = pos.y; y < pos.y + 16; y++) {
        for (int x = pos.x; x < pos.x + width; x++) {
            lfb[y * windowDim.width + x] = entry->bg;
        }
    }

    dxui_rect crop;
    crop.pos = (dxui_pos) {0, 0};
    crop.dim = windowDim;
    dxui_draw_text_wc(context, lfb, entry->wc, entry->fg, pos, crop,
            windowDim.width);

    if (cursorVisible && cursorPos.y == row && cursorPos.x == col) {
        for (int y = pos.y + 14; y < pos.y + 16; y++) {
            for (int x = pos.x; x < pos.x + 8; x++) {
                lfb[y * windowDim.width + x] = entry->fg;
            }
        }
    }
}

static void ensureStdFdsAreUsed(void) {
    for (int fd = 0; fd < 3; fd++) {
        if (fcntl(fd, F_GETFL) < 0) {
            if (open("/dev/null", O_RDWR) != fd) {
                dxui_panic(context, "Failed to open '/dev/null'");
            }
        }
    }
}

static void handleResize(void) {
    resized = false;

    dxui_dim newDim = dxui_get_dim(window);
    struct winsize ws = windowSize;
    ws.ws_col = (newDim.width + 1) / 9;
    ws.ws_row = newDim.height / 16;

    if (ws.ws_col == windowSize.ws_col && ws.ws_row == windowSize.ws_row) {
        return;
    }

    needsRedraw = true;
    newDim.width = ws.ws_col * 9 - 1;
    newDim.height = ws.ws_row * 16;
    lfb = dxui_get_framebuffer(window, newDim);
    if (!lfb) dxui_panic(context, "Failed to create framebuffer");
    windowDim = newDim;

    if (cursorPos.y >= ws.ws_row) {
        scroll(cursorPos.y - ws.ws_row + 1, color, true);
        cursorPos.y = ws.ws_row - 1;
    }

    if (cursorPos.x >= ws.ws_col) {
        cursorPos.x = ws.ws_col - 1;
    }
    if (savedPos.x >= ws.ws_col) {
        savedPos.x = ws.ws_col - 1;
    }
    if (savedPos.y >= ws.ws_row) {
        savedPos.y = ws.ws_row - 1;
    }
    if (alternateSavedPos.x >= ws.ws_col) {
        alternateSavedPos.x = ws.ws_col - 1;
    }
    if (alternateSavedPos.y >= ws.ws_row) {
        alternateSavedPos.y = ws.ws_row - 1;
    }

    TextEntry* buffer = malloc(ws.ws_col * ws.ws_row * sizeof(TextEntry));
    if (!buffer) dxui_panic(context, "malloc");

    TextEntry empty;
    empty.wc = L' ';
    empty.fg = color.fgColor;
    empty.bg = color.bgColor;

    for (int row = 0; row < ws.ws_row; row++) {
        for (int col = 0; col < ws.ws_col; col++) {
            TextEntry* entry = &empty;
            if (row < windowSize.ws_row && col < windowSize.ws_col) {
                entry = &currentBuffer[row * windowSize.ws_col + col];
            }
            buffer[row * ws.ws_col + col] = *entry;
        }
    }

    bool isAlternate = usingAlternateBuffer();

    if (isAlternate) {
        free(alternateBuffer);
        alternateBuffer = buffer;
    } else {
        free(primaryBuffer);
        primaryBuffer = buffer;
    }
    currentBuffer = buffer;

    buffer = malloc(ws.ws_col * ws.ws_row * sizeof(TextEntry));
    if (!buffer) dxui_panic(context, "malloc");

    if (isAlternate) {
        empty.fg = savedColor.fgColor;
        empty.bg = savedColor.bgColor;

        for (int row = 0; row < ws.ws_row; row++) {
            for (int col = 0; col < ws.ws_col; col++) {
                TextEntry* entry = &empty;
                if (row < windowSize.ws_row && col < windowSize.ws_col) {
                    entry = &primaryBuffer[row * windowSize.ws_col + col];
                }
                buffer[row * ws.ws_col + col] = *entry;
            }
        }
        free(primaryBuffer);
        primaryBuffer = buffer;
    } else {
        free(alternateBuffer);
        alternateBuffer = buffer;
    }

    windowSize = ws;
    tcsetwinsize(terminalController, &windowSize);
}

static void handleSigchld(int signo) {
    (void) signo;

    int status;
    wait(&status);
    childExited = 1;
}

static void onClose(dxui_window* window) {
    (void) window;
    exit(0);
}

static void onKey(dxui_control* control, dxui_key_event* event) {
    (void) control;

    if (event->codepoint) {
        char buffer[MB_LEN_MAX];
        int bytes = wctomb(buffer, event->codepoint);
        if (bytes < 0) return;
        writeAll(terminalController, buffer, bytes);
    } else if (event->key > 0) {
        for (size_t i = 0; sequences[i].key != 0; i++) {
            if (sequences[i].key == event->key) {
                writeAll(terminalController, sequences[i].sequence,
                        strlen(sequences[i].sequence));
            }
        }
    }
}

static void onResize(dxui_window* window, dxui_resize_event* event) {
    (void) window; (void) event;
    resized = true;
}

static void printCharacter(char c) {
    if (status == NORMAL && (!mbsinit(&ps) || c != '\e')) {
        printCharacterRaw(c);
        return;
    }

    if (status == NORMAL) {
        status = ESCAPED;
    } else if (status == ESCAPED) {
        if (c == '[') { // CSI - Control Sequence Introducer
            status = CSI;
            for (size_t i = 0; i < MAX_PARAMS; i++) {
                params[i] = 0;
                paramSpecified[i] = false;
            }
            paramIndex = 0;
            questionMarkModifier = false;
        } else if (c == ']') { // OSC - Operating System Command
            status = OSC;
        } else if (c == 'c') { // RIS - Reset to Initial State
            color = defaultColor;
            endOfLine = false;
            fgIsVgaColor = true;
            reversedColors = false;
            CharPos firstPos = {0, 0};
            CharPos lastPos = {windowSize.ws_col - 1, windowSize.ws_row - 1};
            clear(firstPos, lastPos, color);
            cursorPos = (CharPos) {0, 0};
            savedPos = (CharPos) {0, 0};
            status = NORMAL;
        } else if (c == '7') { // Save cursor
            if (usingAlternateBuffer()) {
                alternateSavedColor = color;
                alternateSavedPos = cursorPos;
            } else {
                savedColor = color;
                savedPos = cursorPos;
            }
            status = NORMAL;
        } else if (c == '8') { // Restore cursor
            color = usingAlternateBuffer() ? alternateSavedColor : savedColor;
            cursorPos = usingAlternateBuffer() ? alternateSavedPos : savedPos;
            endOfLine = false;
            status = NORMAL;
        } else {
            // Unknown escape sequence, ignore.
            status = NORMAL;
        }
    } else if (status == CSI) {
        if (c >= '0' && c <= '9') {
            params[paramIndex] = params[paramIndex] * 10 + c - '0';
            paramSpecified[paramIndex] = true;
        } else if (c == '?') {
            questionMarkModifier = true;
        } else if (c == ';') {
            paramIndex++;
            if (paramIndex >= MAX_PARAMS) {
                // Unsupported number of parameters.
                status = NORMAL;
            }
        } else {
            switch (c) {
            case 'A': { // CUU - Cursor Up
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                if (cursorPos.y < param) {
                    cursorPos.y = 0;
                } else {
                    cursorPos.y -= param;
                }
            } break;
            case 'B': { // CUD - Cursor Down
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                if (cursorPos.y + param >= windowSize.ws_row) {
                    cursorPos.y = windowSize.ws_row - 1;
                } else {
                    cursorPos.y += param;
                }
            } break;
            case 'C': { // CUF - Cursor Forward
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                if (cursorPos.x + param >= windowSize.ws_col) {
                    cursorPos.x = windowSize.ws_col - 1;
                } else {
                    cursorPos.x += param;
                }
                endOfLine = false;
            } break;
            case 'D': { // CUB - Cursor Back
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                if (cursorPos.x < param) {
                    cursorPos.x = 0;
                } else {
                    cursorPos.x -= param;
                }
                endOfLine = false;
            } break;
            case 'E': { // CNL - Cursor Next Line
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                if (cursorPos.y + param >= windowSize.ws_row) {
                    cursorPos.y = windowSize.ws_row - 1;
                } else {
                    cursorPos.y += param;
                }
                cursorPos.x = 0;
                endOfLine = false;
            } break;
            case 'F': { // CPL - Cursor Previous Line
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                if (cursorPos.y < param) {
                    cursorPos.y = 0;
                } else {
                    cursorPos.y -= param;
                }
                cursorPos.x = 0;
                endOfLine = false;
            } break;
            case 'G': { // CHA - Cursor Horizontal Absolute
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                if (0 < param && param <= windowSize.ws_col) {
                    cursorPos.x = param - 1;
                }
            } break;
            case 'H':
            case 'f': { // CUP - Cursor Position
                unsigned int x = paramSpecified[1] ? params[1] : 1;
                unsigned int y = paramSpecified[0] ? params[0] : 1;
                if (0 < x && x <= windowSize.ws_col &&
                        0 < y && y <= windowSize.ws_row) {
                    cursorPos = (CharPos) {x - 1, y - 1};
                }
                endOfLine = false;
            } break;
            case 'J': { // ED - Erase Display
                unsigned int param = paramSpecified[0] ? params[0] : 0;
                CharPos firstPos = {0, 0};
                CharPos lastPos = {windowSize.ws_col - 1,
                        windowSize.ws_row - 1};
                if (param == 0) {
                    clear(cursorPos, lastPos, color);
                } else if (param == 1) {
                    clear(firstPos, cursorPos, color);
                } else if (param == 2) {
                    clear(firstPos, lastPos, color);
                }
            } break;
            case 'K': { // EL - Erase in Line
                unsigned int param = paramSpecified[0] ? params[0] : 0;
                CharPos firstPosInLine = {0, cursorPos.y};
                CharPos lastPosInLine = {windowSize.ws_col - 1, cursorPos.y};
                if (param == 0) {
                    clear(cursorPos, lastPosInLine, color);
                } else if (param == 1) {
                    clear(firstPosInLine, cursorPos, color);
                } else if (param == 2) {
                    clear(firstPosInLine, lastPosInLine, color);
                }
            } break;
            case 'S': { // SU - Scroll Up
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                scroll(param, color, true);
            } break;
            case 'T': { // SD - Scroll Down
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                scroll(param, color, false);
            } break;
            case 'd': { // VPA - Line Position Absolute
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                if (0 < param && param <= windowSize.ws_row) {
                    cursorPos.y = param - 1;
                }
            } break;
            case 'h': // SM - Set Mode
                if (!questionMarkModifier) break;
                switch (params[0]) {
                case 25: // Show cursor
                    cursorVisible = true;
                    break;
                case 1049: // Enable alternate screen buffer
                    if (!usingAlternateBuffer()) {
                        savedPos = cursorPos;
                        savedColor = color;
                        cursorPos = alternateSavedPos;
                        color = alternateSavedColor;
                        currentBuffer = alternateBuffer;
                        CharPos firstPos = {0, 0};
                        CharPos lastPos = {windowSize.ws_col - 1,
                                windowSize.ws_row - 1};
                        clear(firstPos, lastPos, color);
                    }
                    break;
                }
                break;
            case 'l': // RM - Reset Mode
                if (!questionMarkModifier) break;
                switch (params[0]) {
                case 25: // Hide cursor
                    cursorVisible = false;
                    break;
                case 1049: // Disable alternate screen buffer
                    if (usingAlternateBuffer()) {
                        alternateSavedPos = cursorPos;
                        alternateSavedColor = color;
                        cursorPos = savedPos;
                        color = savedColor;
                        currentBuffer = primaryBuffer;
                    }
                    break;
                }
                break;
            case 'm': { // SGR - Select Graphic Rendition
                setGraphicsRendition();
            } break;
            case 's': // SCP - Save Cursor Position
                if (usingAlternateBuffer()) {
                    alternateSavedPos = cursorPos;
                } else {
                    savedPos = cursorPos;
                }
                break;
            case 'u': // RCP - Restore Cursor Position
                cursorPos = usingAlternateBuffer() ? alternateSavedPos :
                        savedPos;
                endOfLine = false;
                break;
            default:
                // Unknown command, ignore
                break;
            }
            status = NORMAL;
        }
    } else if (status == OSC) {
        if (c == '\e') {
            status = OSC_ESCAPED;
        } else if (c == '\7') {
            status = NORMAL;
        }
    } else if (status == OSC_ESCAPED) {
        if (c == '\\') {
            status = NORMAL;
        } else {
            status = OSC;
        }
    }
}

static void printCharacterRaw(char c) {
    wchar_t wc;
    size_t result = mbrtowc(&wc, &c, 1, &ps);
    if (result == (size_t) -2) { // incomplete character
        return;
    } else if (result == (size_t) -1) { // invalid character
        memset(&ps, 0, sizeof(ps));
        wc = L'�';
    }

    Color currentColor = color;
    if (reversedColors) {
        currentColor = reverse(color);
    }

    if (wc == L'\b') {
        if (endOfLine) {
            endOfLine = false;
        } else if (cursorPos.x == 0 && cursorPos.y > 0) {
            cursorPos.x = windowSize.ws_col - 1;
            cursorPos.y--;
        } else {
            cursorPos.x--;
        }
        return;
    }

    if (endOfLine || wc == L'\n') {
        cursorPos.x = 0;

        if (cursorPos.y + 1 >= windowSize.ws_row) {
            scroll(1, currentColor, true);
            cursorPos.y = windowSize.ws_row - 1;
        } else {
            cursorPos.y++;
        }
        endOfLine = false;
        if (wc == L'\n') return;
    }

    if (wc == L'\t') {
        unsigned int length = tabsize - cursorPos.x % tabsize;
        CharPos endPos = {cursorPos.x + length - 1, cursorPos.y};
        if (endPos.x >= windowSize.ws_col) {
            endPos.x = windowSize.ws_col - 1;
        }
        clear(cursorPos, endPos, currentColor);
        cursorPos.x += length - 1;
    } else {
        TextEntry entry;
        entry.wc = wc;
        entry.fg = currentColor.fgColor;
        entry.bg = currentColor.bgColor;
        currentBuffer[cursorPos.y * windowSize.ws_col + cursorPos.x] = entry;
    }

    if (cursorPos.x + 1 >= windowSize.ws_col) {
        endOfLine = true;
    } else {
        cursorPos.x++;
    }
}

static void readController(void) {
    char buffer[8 * 4096];

    ssize_t size = read(terminalController, buffer, sizeof(buffer));
    if (size < 0) return;

    for (size_t i = 0; i < (size_t) size; i++) {
        printCharacter(buffer[i]);
    }
}

static Color reverse(Color c) {
    Color result;
    result.fgColor = c.bgColor;
    result.bgColor = c.fgColor;
    result.vgaColor = (c.vgaColor >> 4) | (c.vgaColor << 4);
    return result;
}

static void scroll(unsigned int lines, Color color, bool up) {
    TextEntry empty;
    empty.wc = L' ';
    empty.fg = color.fgColor;
    empty.bg = color.bgColor;

    if (up) {
        for (unsigned int y = 0; y < windowSize.ws_row; y++) {
            for (unsigned int x = 0; x < windowSize.ws_col; x++) {
                TextEntry* entry = &empty;
                if (y < windowSize.ws_row - lines) {
                    entry = &currentBuffer[x + (y + lines) * windowSize.ws_col];
                }
                currentBuffer[x + y * windowSize.ws_col].wc = entry->wc;
                currentBuffer[x + y * windowSize.ws_col].fg = entry->fg;
                currentBuffer[x + y * windowSize.ws_col].bg = entry->bg;
            }
        }
    } else {
        for (unsigned int y = windowSize.ws_row - 1; y < windowSize.ws_row;
                y--) {
            for (unsigned int x = 0; x < windowSize.ws_col; x++) {
                TextEntry* entry = &empty;
                if (y >= lines) {
                    entry = &currentBuffer[x + (y - lines) * windowSize.ws_col];
                }
                currentBuffer[x + y * windowSize.ws_col].wc = entry->wc;
                currentBuffer[x + y * windowSize.ws_col].fg = entry->fg;
                currentBuffer[x + y * windowSize.ws_col].bg = entry->bg;
            }
        }
    }
}

static void setGraphicsRendition(void) {
    for (size_t i = 0; i <= paramIndex; i++) {
        unsigned int param = params[i];
        const uint8_t ansiToVga[] = { 0, 4, 2, 6, 1, 5, 3, 7 };

        if (param == 0) { // Reset
            color = defaultColor;
            fgIsVgaColor = true;
            reversedColors = false;
        } else if (param == 1) { // Increased intensity / Bold
            // When using colors from the VGA palette we implement this as
            // increased intensity. For other colors this is currently ignored.
            color.vgaColor |= 0x08;
            if (fgIsVgaColor) {
                color.fgColor = vgaColors[(color.vgaColor & 0xF) | 0x08];
            }
        } else if (param == 7) { // Reversed colors
            reversedColors = true;
        } else if (param == 22) { // Normal intensity / Not bold
            color.vgaColor &= ~0x08;
            if (fgIsVgaColor) {
                color.fgColor = vgaColors[color.vgaColor & 0x7];
            }
        } else if (param == 27) { // Disable reversed colors
            reversedColors = false;
        } else if (param >= 30 && param <= 37) {
            color.vgaColor = (color.vgaColor & 0xF8) | ansiToVga[param - 30];
            color.fgColor = vgaColors[color.vgaColor & 0x0F];
            fgIsVgaColor = true;
        } else if (param == 38 || param == 48) {
            i++;
            if (i >= MAX_PARAMS) return;
            uint32_t newColor;
            if (params[i] == 2) {
                if (i + 3 >= MAX_PARAMS) return;
                newColor = RGB(params[i + 1], params[i + 2], params[i + 3]);
                i += 3;
            } else if (params[i] == 5) {
                if (i + 1 >= MAX_PARAMS) return;
                i++;
                if (params[i] < 8) {
                    newColor = vgaColors[ansiToVga[params[i]]];
                } else if (params[i] < 16) {
                    newColor = vgaColors[ansiToVga[params[i] - 8] + 8];
                } else if (params[i] < 232) {
                    params[i] -= 16;
                    uint8_t r = params[i] / 36;
                    uint8_t g = params[i] / 6 % 6;
                    uint8_t b = params[i] % 6;
                    const uint8_t value[] = { 0, 95, 135, 175, 215, 255 };
                    newColor = RGB(value[r], value[g], value[b]);
                } else if (params[i] <= 255) {
                    uint8_t value = 8 + 10 * (params[i] - 232);
                    newColor = RGB(value, value, value);
                } else {
                    continue;
                }
            } else {
                continue;
            }

            if (param == 38) {
                color.fgColor = newColor;
                fgIsVgaColor = false;
            } else {
                color.bgColor = newColor;
            }
        } else if (param == 39) {
            color.vgaColor = (color.vgaColor & 0xF8) | 0x07;
            color.fgColor = vgaColors[color.vgaColor & 0x0F];
            fgIsVgaColor = true;
        } else if (param >= 40 && param <= 47) {
            color.vgaColor = (color.vgaColor & 0x0F) |
                    ansiToVga[param - 40] << 4;
            color.bgColor = vgaColors[(color.vgaColor & 0xF0) >> 4];
        } else if (param == 49) {
            color.vgaColor &= 0x0F;
            color.bgColor = vgaColors[(color.vgaColor & 0xF0) >> 4];
        } else if (param >= 90 && param <= 97) {
            color.vgaColor = (color.vgaColor & 0xF0) | ansiToVga[param - 90] |
                    0x8;
            color.fgColor = vgaColors[color.vgaColor & 0x0F];
            fgIsVgaColor = true;
        } else if (param >= 100 && param <= 107) {
            color.vgaColor = (color.vgaColor & 0x0F) |
                    ansiToVga[param - 100] << 4 | 0x80;
            color.bgColor = vgaColors[(color.vgaColor & 0xF0) >> 4];
        }
    }
}

static void shutdown(void) {
    dxui_shutdown(context);
}

static bool writeAll(int fd, const void* buffer, size_t size) {
    const char* buf = buffer;
    size_t written = 0;
    while (written < size) {
        ssize_t bytesWritten = write(fd, buf + written, size - written);
        if (bytesWritten < 0) {
            if (errno != EINTR) return false;
        } else {
            written += bytesWritten;
        }
    }
    return true;
};

int main(void) {
    color = defaultColor;
    savedColor = defaultColor;
    alternateSavedColor = defaultColor;
    fgIsVgaColor = true;
    status = NORMAL;

    atexit(shutdown);
    context = dxui_initialize(DXUI_INIT_NEED_COMPOSITOR);
    if (!context) dxui_panic(NULL, "Failed to initialize dxui");

    dxui_rect rect = {{ -1, -1, 719, 400 }};
    window = dxui_create_window(context, rect, "Terminal", 0);
    if (!window) dxui_panic(context, "Failed to create terminal window");
    dxui_set_background(window, COLOR_BLACK);

    lfb = dxui_get_framebuffer(window, rect.dim);
    if (!lfb) dxui_panic(context, "Failed to create framebuffer");
    windowDim = rect.dim;

    signal(SIGCHLD, handleSigchld);
    createTerminal();

    primaryBuffer = malloc(windowSize.ws_col * windowSize.ws_row *
            sizeof(TextEntry));
    if (!primaryBuffer) dxui_panic(context, "malloc");
    alternateBuffer = malloc(windowSize.ws_col * windowSize.ws_row *
            sizeof(TextEntry));
    if (!alternateBuffer) dxui_panic(context, "malloc");
    currentBuffer = primaryBuffer;

    TextEntry empty;
    empty.wc = L' ';
    empty.fg = vgaColors[7];
    empty.bg = vgaColors[0];

    for (size_t row = 0; row < windowSize.ws_row; row++) {
        for (size_t col = 0; col < windowSize.ws_col; col++) {
            primaryBuffer[row * windowSize.ws_col + col] = empty;
            alternateBuffer[row * windowSize.ws_col + col] = empty;
        }
    }

    dxui_show(window);
    draw();

    dxui_set_event_handler(window, DXUI_EVENT_WINDOW_CLOSE, onClose);
    dxui_set_event_handler(window, DXUI_EVENT_KEY, onKey);
    dxui_set_event_handler(window, DXUI_EVENT_WINDOW_RESIZED, onResize);

    struct pollfd pfd[1 + DXUI_POLL_NFDS];
    pfd[0].fd = terminalController;
    pfd[0].events = POLLIN;
    while (true) {
        int events = dxui_poll(context, pfd, 1, -1);

        if (resized) {
            handleResize();
        }

        if (events == 1 && pfd[0].revents & POLLIN) {
            readController();
            needsRedraw = true;
        } else if (events < 0 && errno == ECONNRESET) {
            exit(1);
        }

        if (childExited) {
            exit(0);
        }

        if (needsRedraw) {
            draw();
        }
    }
}
