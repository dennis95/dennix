/* Copyright (c) 2017, 2018 Dennis Wölfing
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

#include <string.h>
#include <wchar.h>
#include <dennix/kernel/terminaldisplay.h>

TextDisplay* TerminalDisplay::display;

#define MAX_PARAMS 16

static uint8_t color = 0x07; // gray on black
static CharPos cursorPos;
static CharPos savedPos;

static unsigned int params[MAX_PARAMS];
static bool paramSpecified[MAX_PARAMS];
static size_t paramIndex;
static mbstate_t ps;
static bool skipNewline;

static enum {
    NORMAL,
    ESCAPED,
    CSI,
} status = NORMAL;

void TerminalDisplay::backspace() {
    if (cursorPos.x == 0 && cursorPos.y > 0) {
        cursorPos.x = display->width() - 1;
        cursorPos.y--;
    } else {
        cursorPos.x--;
    }
    display->putCharacter(cursorPos, '\0', 0x07);
}

static void setGraphicsRendition() {
    for (size_t i = 0; i < MAX_PARAMS; i++) {
        if (!paramSpecified[i]) continue;

        switch (params[i]) {
        case 0: // Reset
            color = 0x07;
            break;
        case 1: // Increased intensity
            color |= 0x08;
            break;
        case 22: // Normal intensity
            color &= ~0x08;
            break;
        // Foreground colors
        case 30:
            color = (color & 0xF8) | 0x00;
            break;
        case 31:
            color = (color & 0xF8) | 0x04;
            break;
        case 32:
            color = (color & 0xF8) | 0x02;
            break;
        case 33:
            color = (color & 0xF8) | 0x06;
            break;
        case 34:
            color = (color & 0xF8) | 0x01;
            break;
        case 35:
            color = (color & 0xF8) | 0x05;
            break;
        case 36:
            color = (color & 0xF8) | 0x03;
            break;
        case 37: case 39:
            color = (color & 0xF8) | 0x07;
            break;
        // Background colors
        case 40: case 49:
            color = (color & 0x0F) | 0x00;
            break;
        case 41:
            color = (color & 0x0F) | 0x40;
            break;
        case 42:
            color = (color & 0x0F) | 0x20;
            break;
        case 43:
            color = (color & 0x0F) | 0x60;
            break;
        case 44:
            color = (color & 0x0F) | 0x10;
            break;
        case 45:
            color = (color & 0x0F) | 0x50;
            break;
        case 46:
            color = (color & 0x0F) | 0x30;
            break;
        case 47:
            color = (color & 0x0F) | 0x70;
            break;

        default:
            // Unsupported parameter, ignore
            // TODO: Implement more attributes when needed
            break;
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
        } else if (c == 'c') { // RIS - Reset to Initial State
            color = 0x07;
            CharPos lastPos = {display->width() - 1, display->height() - 1};
            display->clear({0, 0}, lastPos, color);
            cursorPos = {0, 0};
            savedPos = {0, 0};
            status = NORMAL;
        } else {
            // Unknown escape sequence, ignore.
            status = NORMAL;
        }
    } else if (status == CSI) {
        if (c >= '0' && c <= '9') {
            params[paramIndex] = params[paramIndex] * 10 + c - '0';
            paramSpecified[paramIndex] = true;
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
                if (cursorPos.y + param >= display->height()) {
                    cursorPos.y = display->height() - 1;
                } else {
                    cursorPos.y += param;
                }
            } break;
            case 'C': { // CUF - Cursor Forward
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                if (cursorPos.x + param >= display->width()) {
                    cursorPos.x = 0;
                } else {
                    cursorPos.x += param;
                }
            } break;
            case 'D': { // CUB - Cursor Back
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                if (cursorPos.x < param) {
                    cursorPos.x = 0;
                } else {
                    cursorPos.x -= param;
                }
            } break;
            case 'E': { // CNL - Cursor Next Line
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                if (cursorPos.y + param >= display->height()) {
                    cursorPos.y = display->height() - 1;
                } else {
                    cursorPos.y += param;
                }
                cursorPos.x = 0;
            } break;
            case 'F': { // CPL - Cursor Previous Line
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                if (cursorPos.y < param) {
                    cursorPos.y = 0;
                } else {
                    cursorPos.y -= param;
                }
                cursorPos.x = 0;
            } break;
            case 'G': { // CHA - Cursor Horizontal Absolute
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                if (0 < param && param <= display->width()) {
                    cursorPos.x = param - 1;
                }
            } break;
            case 'H':
            case 'f': { // CUP - Cursor Position
                unsigned int x = paramSpecified[1] ? params[1] : 1;
                unsigned int y = paramSpecified[0] ? params[0] : 1;
                if (0 < x && x <= display->width() &&
                        0 < y && y <= display->height()) {
                    cursorPos = {x - 1, y - 1};
                }
            } break;
            case 'J': { // ED - Erase Display
                unsigned int param = paramSpecified[0] ? params[0] : 0;
                CharPos lastPos = {display->width() - 1, display->height() - 1};
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
                CharPos lastPosInLine = {display->width() - 1, cursorPos.y};
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
                if (0 < param && param < display->height()) {
                    cursorPos.y = param - 1;
                }
            } break;
            case 'm': { // SGR - Select Graphic Rendition
                setGraphicsRendition();
            } break;
            case 's': { // SCP - Save Cursor Position
                savedPos = cursorPos;
            } break;
            case 'u': { // RCP - Restore Cursor Position
                cursorPos = savedPos;
            } break;
            default:
                // Unknown command, ignore
                // TODO: Implement more escape sequences when needed
                break;
            }
            status = NORMAL;
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

    if (wc != L'\n') {
        display->putCharacter(cursorPos, wc, color);
    } else if (skipNewline) {
        skipNewline = false;
        return;
    }

    if (wc == L'\n' || cursorPos.x + 1 >= display->width()) {
        cursorPos.x = 0;

        if (cursorPos.y + 1 >= display->height()) {
            display->scroll(1, color);
            cursorPos.y = display->height() - 1;
        } else {
            cursorPos.y++;
        }

        skipNewline = wc != L'\n';
    } else {
        cursorPos.x++;
        skipNewline = false;
    }
}

void TerminalDisplay::updateCursorPosition() {
    display->setCursorPos(cursorPos);
}
