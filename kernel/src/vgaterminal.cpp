/* Copyright (c) 2017 Dennis Wölfing
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

/* kernel/src/vgaterminal.cpp
 * VGA Terminal with support for escape codes as specified by ECMA-48.
 */

#include <string.h>
#include <dennix/kernel/kernel.h>
#include <dennix/kernel/vgaterminal.h>

#define HEIGHT 25
#define WIDTH 80
#define VIDEO_SIZE 2 * WIDTH * HEIGHT
#define MAX_PARAMS 16

// In _start the video memory is mapped at this address.
static char* const video = (char*) 0xC0000000;
static uint8_t color = 0x07; // gray on black
static unsigned int cursorPosX = 0;
static unsigned int cursorPosY = 0;

static unsigned int savedX = 0;
static unsigned int savedY = 0;

static unsigned int params[MAX_PARAMS];
static bool paramSpecified[MAX_PARAMS];
static size_t paramIndex;

static enum {
    NORMAL,
    ESCAPED,
    CSI,
} status = NORMAL;

static inline char* videoOffset(unsigned int line, unsigned int column) {
    return video + 2 * line * WIDTH + 2 * column;
}

void VgaTerminal::backspace() {
    if (cursorPosX == 0 && cursorPosY > 0) {
        cursorPosX = WIDTH - 1;
        cursorPosY--;
    } else {
        cursorPosX--;
    }
    *videoOffset(cursorPosY, cursorPosX) = 0;
    *(videoOffset(cursorPosY, cursorPosX) + 1) = 0;
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

void VgaTerminal::printCharacter(char c) {
    if (likely(status == NORMAL && c != '\e')) {
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
            memset(video, 0, VIDEO_SIZE);
            cursorPosX = cursorPosY = 0;
            savedX = savedY = 0;
            color = 0x07;
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
                if (cursorPosY < param) {
                    cursorPosY = 0;
                } else {
                    cursorPosY -= param;
                }
            } break;
            case 'B': { // CUD - Cursor Down
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                if (cursorPosY + param >= HEIGHT) {
                    cursorPosY = HEIGHT - 1;
                } else {
                    cursorPosY += param;
                }
            } break;
            case 'C': { // CUF - Cursor Forward
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                if (cursorPosX + param >= WIDTH) {
                    cursorPosX = 0;
                } else {
                    cursorPosX += param;
                }
            } break;
            case 'D': { // CUB - Cursor Back
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                if (cursorPosX < param) {
                    cursorPosX = 0;
                } else {
                    cursorPosX -= param;
                }
            } break;
            case 'E': { // CNL - Cursor Next Line
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                if (cursorPosY + param >= HEIGHT) {
                    cursorPosY = HEIGHT - 1;
                } else {
                    cursorPosY += param;
                }
                cursorPosX = 0;
            } break;
            case 'F': { // CPL - Cursor Previous Line
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                if (cursorPosY < param) {
                    cursorPosY = 0;
                } else {
                    cursorPosY -= param;
                }
                cursorPosX = 0;
            } break;
            case 'G': { // CHA - Cursor Horizontal Absolute
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                if (0 < param && param < WIDTH) {
                    cursorPosX = param - 1;
                }
            } break;
            case 'H':
            case 'f': { // CUP - Cursor Position
                unsigned int x = paramSpecified[1] ? params[1] : 1;
                unsigned int y = paramSpecified[0] ? params[0] : 1;
                if (0 < x && x < WIDTH && 0 < y && y < HEIGHT) {
                    cursorPosX = x - 1;
                    cursorPosY = y - 1;
                }
            } break;
            case 'J': { // ED - Erase Display
                unsigned int param = paramSpecified[0] ? params[0] : 0;
                if (param == 0) {
                    char* from = videoOffset(cursorPosY, cursorPosX);
                    memset(from, 0, VIDEO_SIZE - (from - video));
                } else if (param == 1) {
                    char* to = videoOffset(cursorPosY, cursorPosX);
                    memset(video, 0, to - video);
                } else if (param == 2) {
                    memset(video, 0, VIDEO_SIZE);
                }
            } break;
            case 'K': { // EL - Erase in Line
                unsigned int param = paramSpecified[0] ? params[0] : 0;
                if (param == 0) {
                    char* from = videoOffset(cursorPosY, cursorPosX);
                    memset(from, 0, videoOffset(cursorPosY + 1, 0) - from);
                } else if (param == 1) {
                    char* from = videoOffset(cursorPosY, 0);
                    char* to = videoOffset(cursorPosY, cursorPosX);
                    memset(from, 0, to - from);
                } else if (param == 2) {
                    memset(videoOffset(cursorPosY, 0), 0, 2 * WIDTH);
                }
            } break;
            case 'S': { // SU - Scroll Up
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                memmove(video, videoOffset(param, 0),
                        2 * (HEIGHT - param) * WIDTH);
                memset(videoOffset(HEIGHT - param, 0), 0, 2 * param * WIDTH);
            } break;
            case 'T': { // SD - Scroll Down
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                memmove(videoOffset(param, 0), video,
                        2 * (HEIGHT - param) * WIDTH);
                memset(video, 0, 2 * WIDTH * param);
            } break;
            case 'd': { // VPA - Line Position Absolute
                unsigned int param = paramSpecified[0] ? params[0] : 1;
                if (0 < param && param < HEIGHT) {
                    cursorPosY = param - 1;
                }
            } break;
            case 'm': { // SGR - Select Graphic Rendition
                setGraphicsRendition();
            } break;
            case 's': { // SCP - Save Cursor Position
                savedX = cursorPosX;
                savedY = cursorPosY;
            } break;
            case 'u': { // RCP - Restore Cursor Position
                cursorPosX = savedX;
                cursorPosY = savedY;
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

void VgaTerminal::printCharacterRaw(char c) {
    if (c == '\0') {
        // HACK: Clear the screen and reset cursor position when a null
        // character is written. This makes printing to the screen in snake
        // much faster because it doesn't need to move all the lines up.
        cursorPosX = 0;
        cursorPosY = 0;
        memset(video, 0, VIDEO_SIZE);
        return;
    }
    if (c == '\n' || cursorPosX > WIDTH - 1) {
        cursorPosX = 0;
        cursorPosY++;

        if (cursorPosY > HEIGHT - 1) {

            // Move every line up by one
            memmove(video, videoOffset(1, 0), 2 * (HEIGHT - 1) * WIDTH);

            // Clean the last line
            memset(videoOffset(HEIGHT - 1, 0), 0, 2 * WIDTH);

            cursorPosY = HEIGHT - 1;
        }

        if (c == '\n') return;
    }

    *videoOffset(cursorPosY, cursorPosX) = c;
    *(videoOffset(cursorPosY, cursorPosX) + 1) = color;

    cursorPosX++;
}
