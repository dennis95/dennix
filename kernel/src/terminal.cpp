/* Copyright (c) 2016, Dennis Wölfing
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

/* kernel/src/terminal.cpp
 * Terminal class.
 */

#include <dennix/kernel/terminal.h>

Terminal terminal;

// In _start the video memory is mapped at this address.
static char* video = (char*) 0xC0000000;
static int cursorPosX = 0;
static int cursorPosY = 0;

static void printCharacter(char c);

ssize_t Terminal::write(const void* buffer, size_t size) {
    const char* buf = (const char*) buffer;

    for (size_t i = 0; i < size; i++) {
        printCharacter(buf[i]);
    }

    return (ssize_t) size;
}

static void printCharacter(char c) {
    if (c == '\n' || cursorPosX > 79) {
        cursorPosX = 0;
        cursorPosY++;

        if (cursorPosY > 24) {

            // Move every line up by one
            for (size_t i = 0; i < 2 * 24 * 80; i++) {
                video[i] = video[i + 2 * 80];
            }

            // Clean the last line
            for (size_t i = 2 * 24 * 80; i < 2 * 25 * 80; i++) {
                video[i] = 0;
            }

            cursorPosY = 24;
        }

        if (c == '\n') return;
    }

    video[cursorPosY * 2 * 80 + 2 * cursorPosX] = c;
    video[cursorPosY * 2 * 80 + 2 * cursorPosX + 1] = 0x07; // gray on black

    cursorPosX++;
}
