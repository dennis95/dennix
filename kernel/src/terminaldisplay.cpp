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

/* kernel/src/terminaldisplay.cpp
 * Terminal display with support for ECMA-48 terminal escapes.
 */

#include <signal.h>
#include <string.h>
#include <wchar.h>
#include <dennix/display.h>
#include <dennix/kernel/terminal.h>
#include <dennix/kernel/terminaldisplay.h>

Reference<Display> TerminalDisplay::display;

#define MAX_PARAMS 16

static const uint32_t vgaColors[16] = {
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
    .fgColor = vgaColors[7],
    .bgColor = vgaColors[0],
    .vgaColor = 0x07
};
static bool alternateBuffer = false;
static Color color = defaultColor;
static Color savedColor = defaultColor;
static Color alternateSavedColor = defaultColor;
static bool fgIsVgaColor = true;
static CharPos cursorPos;
static CharPos savedPos;
static CharPos alternateSavedPos;

static bool endOfLine;
static unsigned int params[MAX_PARAMS];
static bool paramSpecified[MAX_PARAMS];
static size_t paramIndex;
static mbstate_t ps;
static bool questionMarkModifier;
static const unsigned int tabsize = 8;

static enum {
    NORMAL,
    ESCAPED,
    CSI,
    OSC,
    OSC_ESCAPED,
} status = NORMAL;

void TerminalDisplay::backspace() {
    // TODO: When the deleted character was a tab the cursor needs to be moved
    // by an unknown number of positions but we do not keep track of this
    // information.
    if (endOfLine) {
        endOfLine = false;
    } else if (cursorPos.x == 0 && cursorPos.y > 0) {
        cursorPos.x = display->columns - 1;
        cursorPos.y--;
    } else {
        cursorPos.x--;
    }
    display->putCharacter(cursorPos, '\0', color);
}

static void setGraphicsRendition() {
    for (size_t i = 0; i <= paramIndex; i++) {
        unsigned int param = params[i];
        const uint8_t ansiToVga[] = { 0, 4, 2, 6, 1, 5, 3, 7 };

        if (param == 0) { // Reset
            color = defaultColor;
            fgIsVgaColor = true;
        } else if (param == 1) { // Increased intensity / Bold
            // When using colors from the VGA palette we implement this as
            // increased intensity. For other colors this is currently ignored.
            color.vgaColor |= 0x08;
            if (fgIsVgaColor) {
                color.fgColor = vgaColors[(color.vgaColor & 0xF) | 0x08];
            }
        } else if (param == 22) { // Normal intensity / Not bold
            color.vgaColor &= ~0x08;
            if (fgIsVgaColor) {
                color.fgColor = vgaColors[color.vgaColor & 0x7];
            }
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
                if (params[i] < 16) {
                    newColor = vgaColors[params[i]];
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

void TerminalDisplay::printCharacter(char c) {
    if (likely(status == NORMAL && (!mbsinit(&ps) || c != '\e'))) {
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
            CharPos lastPos = {display->columns - 1, display->rows - 1};
            display->clear({0, 0}, lastPos, color);
            cursorPos = {0, 0};
            savedPos = {0, 0};
            status = NORMAL;
        } else if (c == '7') { // Save cursor
            if (alternateBuffer) {
                alternateSavedColor = color;
                alternateSavedPos = cursorPos;
            } else {
                savedColor = color;
                savedPos = cursorPos;
            }
        } else if (c == '8') { // Restore cursor
            color = alternateBuffer ? alternateSavedColor : savedColor;
            cursorPos = alternateBuffer ? alternateSavedPos : savedPos;
            endOfLine = false;
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
                if (cursorPos.y + param >= display->rows) {
                    cursorPos.y = display->rows - 1;
                } else {
                    cursorPos.y += param;
                }
            } break;
            case 'C': { // CUF - Cursor Forward
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                if (cursorPos.x + param >= display->columns) {
                    cursorPos.x = display->columns - 1;
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
                if (cursorPos.y + param >= display->rows) {
                    cursorPos.y = display->rows - 1;
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
                if (0 < param && param <= display->columns) {
                    cursorPos.x = param - 1;
                }
            } break;
            case 'H':
            case 'f': { // CUP - Cursor Position
                unsigned int x = paramSpecified[1] ? params[1] : 1;
                unsigned int y = paramSpecified[0] ? params[0] : 1;
                if (0 < x && x <= display->columns &&
                        0 < y && y <= display->rows) {
                    cursorPos = {x - 1, y - 1};
                }
                endOfLine = false;
            } break;
            case 'J': { // ED - Erase Display
                unsigned int param = paramSpecified[0] ? params[0] : 0;
                CharPos lastPos = {display->columns - 1, display->rows - 1};
                if (param == 0) {
                    display->clear(cursorPos, lastPos, color);
                } else if (param == 1) {
                    display->clear({0, 0}, cursorPos, color);
                } else if (param == 2) {
                    display->clear({0, 0}, lastPos, color);
                }
            } break;
            case 'K': { // EL - Erase in Line
                unsigned int param = paramSpecified[0] ? params[0] : 0;
                CharPos lastPosInLine = {display->columns - 1, cursorPos.y};
                if (param == 0) {
                    display->clear(cursorPos, lastPosInLine, color);
                } else if (param == 1) {
                    display->clear({0, cursorPos.y}, cursorPos, color);
                } else if (param == 2) {
                    display->clear({0, cursorPos.y}, lastPosInLine, color);
                }
            } break;
            case 'S': { // SU - Scroll Up
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                display->scroll(param, color);
            } break;
            case 'T': { // SD - Scroll Down
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                display->scroll(param, color, false);
            } break;
            case 'd': { // VPA - Line Position Absolute
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                if (0 < param && param <= display->rows) {
                    cursorPos.y = param - 1;
                }
            } break;
            case 'h': // SM - Set Mode
                if (!questionMarkModifier) break;
                switch (params[0]) {
                case 25: // Show cursor
                    display->setCursorVisibility(true);
                    break;
                case 1049: // Enable alternate screen buffer
                    if (!alternateBuffer) {
                        savedPos = cursorPos;
                        savedColor = color;
                        cursorPos = alternateSavedPos;
                        color = alternateSavedColor;
                        display->switchBuffer(color);
                        alternateBuffer = true;
                    }
                    break;
                }
                break;
            case 'l': // RM - Reset Mode
                if (!questionMarkModifier) break;
                switch (params[0]) {
                case 25: // Hide cursor
                    display->setCursorVisibility(false);
                    break;
                case 1049: // Disable alternate screen buffer
                    if (alternateBuffer) {
                        alternateSavedPos = cursorPos;
                        alternateSavedColor = color;
                        cursorPos = savedPos;
                        color = savedColor;
                        display->switchBuffer(color);
                        alternateBuffer = false;
                    }
                    break;
                }
                break;
            case 'm': { // SGR - Select Graphic Rendition
                setGraphicsRendition();
            } break;
            case 's': // SCP - Save Cursor Position
                if (alternateBuffer) {
                    alternateSavedPos = cursorPos;
                } else {
                    savedPos = cursorPos;
                }
                break;
            case 'u': // RCP - Restore Cursor Position
                cursorPos = alternateBuffer ? alternateSavedPos : savedPos;
                endOfLine = false;
                break;
            default:
                // Unknown command, ignore
                // TODO: Implement more escape sequences when needed
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

void TerminalDisplay::printCharacterRaw(char c) {
    wchar_t wc;
    size_t result = mbrtowc(&wc, &c, 1, &ps);
    if (result == (size_t) -2) { // incomplete character
        return;
    } else if (result == (size_t) -1) { // invalid character
        ps = {};
        wc = L'�';
    }

    if (endOfLine || wc == L'\n') {
        cursorPos.x = 0;

        if (cursorPos.y + 1 >= display->rows) {
            display->scroll(1, color);
            cursorPos.y = display->rows - 1;
        } else {
            cursorPos.y++;
        }
        endOfLine = false;
        if (wc == L'\n') return;
    }

    if (wc == L'\t') {
        unsigned int length = tabsize - cursorPos.x % tabsize;
        CharPos endPos = {cursorPos.x + length - 1, cursorPos.y};
        if (endPos.x >= display->columns) {
            endPos.x = display->columns - 1;
        }
        display->clear(cursorPos, endPos, color);
        cursorPos.x += length - 1;
    } else {
        display->putCharacter(cursorPos, wc, color);
    }

    if (cursorPos.x + 1 >= display->columns) {
        endOfLine = true;
    } else {
        cursorPos.x++;
    }
}

void TerminalDisplay::updateCursorPosition() {
    display->setCursorPos(cursorPos);
}

void TerminalDisplay::updateDisplaySize() {
    if (cursorPos.x >= display->columns) {
        cursorPos.x = display->columns - 1;
    }
    if (cursorPos.y >= display->rows) {
        cursorPos.y = display->rows - 1;
    }
    if (savedPos.x >= display->columns) {
        savedPos.x = display->columns - 1;
    }
    if (savedPos.y >= display->rows) {
        savedPos.y = display->rows - 1;
    }

    updateCursorPosition();
    terminal->raiseSignal(SIGWINCH);
}
