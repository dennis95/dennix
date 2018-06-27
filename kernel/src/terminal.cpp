/* Copyright (c) 2016, 2017, 2018 Dennis Wölfing
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

#include <errno.h>
#include <sched.h>
#include <dennix/kernel/kernel.h>
#include <dennix/kernel/signal.h>
#include <dennix/kernel/terminal.h>
#include <dennix/kernel/vgaterminal.h>

#define CTRL(c) ((c) & 0x1F)

static Terminal _terminal;
Reference<Terminal> terminal(&_terminal);

Terminal::Terminal() : Vnode(S_IFCHR, 0) {
    termio.c_iflag = 0;
    termio.c_oflag = 0;
    termio.c_cflag = 0;
    termio.c_lflag = ECHO | ICANON;

    termio.c_cc[VEOF] = CTRL('D');
    termio.c_cc[VEOL] = 0;
    termio.c_cc[VERASE] = '\b';
    termio.c_cc[VINTR] = CTRL('C');
    termio.c_cc[VKILL] = CTRL('U');
    termio.c_cc[VMIN] = 1;
    termio.c_cc[VQUIT] = CTRL('\\');
    termio.c_cc[VSTART] = CTRL('Q');
    termio.c_cc[VSTOP] = CTRL('S');
    termio.c_cc[VSUSP] = CTRL('Z');
    termio.c_cc[VTIME] = 0;

    numEof = 0;
}

void Terminal::handleCharacter(char c) {
    if (termio.c_lflag & ICANON) {
        // TODO: Handle the unimplemented control characters.
        if (c == termio.c_cc[VEOF]) {
            if (terminalBuffer.hasIncompleteLine()) {
                terminalBuffer.endLine();
            } else {
                numEof++;
            }
        } else if (c == termio.c_cc[VEOL]) {

        } else if (c == termio.c_cc[VERASE]) {
            if (terminalBuffer.backspace() && (termio.c_lflag & ECHO)) {
                VgaTerminal::backspace();
            }
        } else if (c == termio.c_cc[VINTR]) {

        } else if (c == termio.c_cc[VKILL]) {

        } else if (c == termio.c_cc[VQUIT]) {

        } else if (c == termio.c_cc[VSTART]) {

        } else if (c == termio.c_cc[VSTOP]) {

        } else if (c == termio.c_cc[VSUSP]) {

        } else {
            if (termio.c_lflag & ECHO) {
                VgaTerminal::printCharacterRaw(c);
            }
            terminalBuffer.write(c);
            if (c == '\n') {
                terminalBuffer.endLine();
            }
        }
    } else {
        if (termio.c_lflag & ECHO) {
            VgaTerminal::printCharacterRaw(c);
        }
        terminalBuffer.write(c);
        terminalBuffer.endLine();
    }
}

void Terminal::handleSequence(const char* sequence) {
    if (!(termio.c_lflag & ICANON)) {
        while (*sequence) {
            if (termio.c_lflag & ECHO) {
                VgaTerminal::printCharacterRaw(*sequence);
            }
            terminalBuffer.write(*sequence++);
        }
        terminalBuffer.endLine();
    }
}

void Terminal::onKeyboardEvent(int key) {
    char c = Keyboard::getCharFromKey(key);
    if (c) {
        handleCharacter(c);
    } else {
        const char* sequence = Keyboard::getSequenceFromKey(key);
        if (sequence) {
            handleSequence(sequence);
        }
    }
    VgaTerminal::updateCursorPosition();
}

int Terminal::isatty() {
    return 1;
}

ssize_t Terminal::read(void* buffer, size_t size) {
    if (size == 0) return 0;
    char* buf = (char*) buffer;
    size_t readSize = 0;

    while (readSize < size) {
        while (!terminalBuffer.available() && !numEof) {
            if (termio.c_lflag & ICANON) {
                if (readSize) {
                    updateTimestamps(true, false, false);
                    return readSize;
                }
            } else {
                if (readSize >= termio.c_cc[VMIN]) {
                    updateTimestamps(true, false, false);
                    return readSize;
                }
            }
            sched_yield();

            if (Signal::isPending()) {
                if (readSize) {
                    updateTimestamps(true, false, false);
                    return readSize;
                }
                errno = EINTR;
                return -1;
            }
        }

        if (numEof) {
            if (readSize) {
                updateTimestamps(true, false, false);
                return readSize;
            }
            numEof--;
            return 0;
        }
        char c = terminalBuffer.read();
        buf[readSize] = c;
        readSize++;
        if ((termio.c_lflag & ICANON) && c == '\n') break;
    }

    updateTimestamps(true, false, false);
    return (ssize_t) readSize;
}

int Terminal::tcgetattr(struct termios* result) {
    *result = termio;
    return 0;
}

int Terminal::tcsetattr(int flags, const struct termios* termio) {
    this->termio = *termio;

    if (flags == TCSAFLUSH) {
        terminalBuffer.reset();
        numEof = 0;
    }
    return 0;
}

ssize_t Terminal::write(const void* buffer, size_t size) {
    if (size == 0) return 0;
    AutoLock lock(&mutex);
    const char* buf = (const char*) buffer;

    for (size_t i = 0; i < size; i++) {
        VgaTerminal::printCharacter(buf[i]);
    }
    VgaTerminal::updateCursorPosition();

    updateTimestamps(false, true, true);
    return (ssize_t) size;
}

// TODO: TerminalBuffer reads and writes should be atomic.

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

void TerminalBuffer::endLine() {
    lineIndex = writeIndex;
}

bool TerminalBuffer::hasIncompleteLine() {
    return lineIndex != writeIndex;
}

char TerminalBuffer::read() {
    char result = circularBuffer[readIndex];
    readIndex = (readIndex + 1) % TERMINAL_BUFFER_SIZE;
    return result;
}

void TerminalBuffer::reset() {
    readIndex = 0;
    lineIndex = 0;
    writeIndex = 0;
}

void TerminalBuffer::write(char c) {
    while ((writeIndex + 1) % TERMINAL_BUFFER_SIZE == readIndex);
    circularBuffer[writeIndex] = c;
    writeIndex = (writeIndex + 1) % TERMINAL_BUFFER_SIZE;
}
