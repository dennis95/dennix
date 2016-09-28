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

#include <dennix/stat.h>
#include <dennix/kernel/kernel.h>
#include <dennix/kernel/terminal.h>

Terminal terminal;

// In _start the video memory is mapped at this address.
static char* video = (char*) 0xC0000000;
static int cursorPosX = 0;
static int cursorPosY = 0;

static void printCharacter(char c);

Terminal::Terminal() : Vnode(S_IFCHR) {

}

void Terminal::onKeyboardEvent(int key) {
    char c = Keyboard::getCharFromKey(key);

    if (c == '\b') {
        if (terminalBuffer.backspace()) {
            cursorPosX--;
            if (cursorPosX < 0) {
                cursorPosX = 79;
                cursorPosY--;
            }
            video[cursorPosY * 2 * 80 + 2 * cursorPosX] = 0;
            video[cursorPosY * 2 * 80 + 2 * cursorPosX + 1] = 0;
        }

    } else if (c) {
        printCharacter(c);
        terminalBuffer.write(c);
    }
}

ssize_t Terminal::read(void* buffer, size_t size) {
    char* buf = (char*) buffer;
    for (size_t i = 0; i < size; i++) {
        buf[i] = terminalBuffer.read();
    }

    return (ssize_t) size;
}

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

TerminalBuffer::TerminalBuffer() {
    readIndex = 0;
    lineIndex = 0;
    writeIndex = 0;
}

bool TerminalBuffer::backspace() {
    if (lineIndex == writeIndex) return false;

    if (likely(writeIndex != 0)) {
        writeIndex = (writeIndex - 1) % TERMINAL_BUFFER_SIZE;
    } else {
        writeIndex = TERMINAL_BUFFER_SIZE - 1;
    }
    return true;
}

char TerminalBuffer::read() {
    while (readIndex == lineIndex);
    char result = circularBuffer[readIndex];
    readIndex = (readIndex + 1) % TERMINAL_BUFFER_SIZE;
    return result;
}

void TerminalBuffer::write(char c) {
    while ((writeIndex + 1) % TERMINAL_BUFFER_SIZE == readIndex);
    circularBuffer[writeIndex] = c;
    writeIndex = (writeIndex + 1) % TERMINAL_BUFFER_SIZE;
    if (c == '\n') lineIndex = writeIndex;
}
