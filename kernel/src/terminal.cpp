/* Copyright (c) 2016, 2017 Dennis Wölfing
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

#include <dennix/kernel/kernel.h>
#include <dennix/kernel/terminal.h>
#include <dennix/kernel/vgaterminal.h>

Terminal terminal;

Terminal::Terminal() : Vnode(S_IFCHR) {
    termio.c_lflag = ECHO | ICANON;
    termio.c_cc[VMIN] = 1;
}

void Terminal::onKeyboardEvent(int key) {
    char c = Keyboard::getCharFromKey(key);

    if ((termio.c_lflag & ICANON) && c == '\b') {
        if (terminalBuffer.backspace() && (termio.c_lflag & ECHO)) {
            VgaTerminal::backspace();
        }

    } else if (c) {
        if (termio.c_lflag & ECHO) {
            VgaTerminal::printCharacterRaw(c);
        }
        terminalBuffer.write(c, termio.c_lflag & ICANON);
    }
}

ssize_t Terminal::read(void* buffer, size_t size) {
    char* buf = (char*) buffer;
    if (termio.c_cc[VMIN] == 0 && !terminalBuffer.available()) return 0;
    while (termio.c_cc[VMIN] > terminalBuffer.available());

    for (size_t i = 0; i < size; i++) {
        buf[i] = terminalBuffer.read();
    }

    return (ssize_t) size;
}

int Terminal::tcgetattr(struct termios* result) {
    *result = termio;
    return 0;
}

int Terminal::tcsetattr(int flags, const struct termios* termio) {
    this->termio = *termio;

    if (flags == TCSAFLUSH) {
        terminalBuffer.reset();
    }
    return 0;
}

ssize_t Terminal::write(const void* buffer, size_t size) {
    const char* buf = (const char*) buffer;

    for (size_t i = 0; i < size; i++) {
        VgaTerminal::printCharacter(buf[i]);
    }

    return (ssize_t) size;
}

TerminalBuffer::TerminalBuffer() {
    reset();
}

size_t TerminalBuffer::available() {
    return lineIndex >= readIndex ?
            lineIndex - readIndex : readIndex - lineIndex;
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

void TerminalBuffer::reset() {
    readIndex = 0;
    lineIndex = 0;
    writeIndex = 0;
}

void TerminalBuffer::write(char c, bool canonicalMode) {
    while ((writeIndex + 1) % TERMINAL_BUFFER_SIZE == readIndex);
    circularBuffer[writeIndex] = c;
    writeIndex = (writeIndex + 1) % TERMINAL_BUFFER_SIZE;
    if (c == '\n' || !canonicalMode) {
        lineIndex = writeIndex;
    }
}
