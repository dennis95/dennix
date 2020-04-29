/* Copyright (c) 2016, 2017, 2018, 2019, 2020 Dennis Wölfing
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
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <dennix/devctls.h>
#include <dennix/kernel/kernel.h>
#include <dennix/kernel/process.h>
#include <dennix/kernel/signal.h>
#include <dennix/kernel/terminal.h>
#include <dennix/kernel/terminaldisplay.h>

#define CTRL(c) ((c) & 0x1F)

static Terminal _terminal;
Reference<Terminal> terminal(&_terminal);

Terminal::Terminal() : Vnode(S_IFCHR, 0) {
    termio.c_iflag = 0;
    termio.c_oflag = 0;
    termio.c_cflag = 0;
    termio.c_lflag = ECHO | ICANON | ISIG;

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

    foregroundGroup = -1;
    numEof = 0;
}

void Terminal::handleCharacter(char c) {
    if (termio.c_lflag & ICANON && c == termio.c_cc[VEOF]) {
        if (terminalBuffer.hasIncompleteLine()) {
            terminalBuffer.endLine();
        } else {
            numEof++;
        }
    } else if (termio.c_lflag & ICANON && c == termio.c_cc[VERASE]) {
        if (terminalBuffer.backspace() && (termio.c_lflag & ECHO)) {
            TerminalDisplay::backspace();
        }
    } else if (termio.c_lflag & ISIG && c == termio.c_cc[VINTR]) {
        raiseSignal(SIGINT);
    } else if (termio.c_lflag & ICANON && c == termio.c_cc[VKILL]) {

    } else if (termio.c_lflag & ISIG && c == termio.c_cc[VQUIT]) {
        raiseSignal(SIGQUIT);
    } else if (/* IXON */ false && c == termio.c_cc[VSTART]) {

    } else if (/* IXON */ false && c == termio.c_cc[VSTOP]) {

    } else if (termio.c_lflag & ISIG && c == termio.c_cc[VSUSP]) {

    } else {
        if (termio.c_lflag & ECHO || (termio.c_lflag & ECHONL && c == '\n')) {
            TerminalDisplay::printCharacterRaw(c);
        }
        terminalBuffer.write(c);
        if (!(termio.c_lflag & ICANON) || c == '\n' || c == termio.c_cc[VEOL]) {
            terminalBuffer.endLine();
        }
    }
}

void Terminal::handleSequence(const char* sequence) {
    if (!(termio.c_lflag & ICANON)) {
        while (*sequence) {
            if (termio.c_lflag & ECHO) {
                TerminalDisplay::printCharacterRaw(*sequence);
            }
            terminalBuffer.write(*sequence++);
        }
        terminalBuffer.endLine();
    }
}

void Terminal::onKeyboardEvent(int key) {
    char buffer[MB_CUR_MAX];
    size_t bytes = Keyboard::getUtf8FromKey(key, buffer);
    if (bytes != (size_t) -1) {
        for (size_t i = 0; i < bytes; i++) {
            handleCharacter(buffer[i]);
        }
    } else {
        const char* sequence = Keyboard::getSequenceFromKey(key);
        if (sequence) {
            handleSequence(sequence);
        }
    }
    TerminalDisplay::updateCursorPosition();
}

int Terminal::devctl(int command, void* restrict data, size_t size,
        int* restrict info) {
    switch (command) {
    case TIOCGPGRP: {
        if (size != 0 && size != sizeof(pid_t)) {
            *info = -1;
            return EINVAL;
        }

        if (Process::current()->controllingTerminal != this) {
            *info = -1;
            return ENOTTY;
        }

        pid_t* pgid = (pid_t*) data;

        if (foregroundGroup >= 0) {
            *pgid = foregroundGroup;
        } else {
            *pgid = INT_MAX;
        }
        *info = 0;
        return 0;
    } break;
    case TIOCGWINSZ: {
        if (size != 0 && size != sizeof(struct winsize)) {
            *info = -1;
            return EINVAL;
        }

        struct winsize* ws = (struct winsize*) data;
        ws->ws_col = TerminalDisplay::display->columns;
        ws->ws_row = TerminalDisplay::display->rows;
        *info = 0;
        return 0;
    } break;
    case TIOCSPGRP: {
        if (size != 0 && size != sizeof(pid_t)) {
            *info = -1;
            return EINVAL;
        }

        if (Process::current()->controllingTerminal != this) {
            *info = -1;
            return ENOTTY;
        }

        const pid_t* pgid = (const pid_t*) data;

        if (*pgid < 0) {
            *info = -1;
            return EINVAL;
        }

        if (!Process::getGroup(*pgid)) {
            *info = -1;
            return EPERM;
        }

        // TODO: The terminal should lose its foreground ground when the group
        // dies.
        foregroundGroup = *pgid;

        *info = 0;
        return 0;
    } break;
    default:
        *info = -1;
        return EINVAL;
    }
}

int Terminal::isatty() {
    return 1;
}

void Terminal::raiseSignal(int signal) {
    siginfo_t siginfo = {};
    siginfo.si_signo = signal;
    siginfo.si_code = SI_KERNEL;

    if (foregroundGroup > 0) {
        Process* group = Process::getGroup(foregroundGroup);
        if (group) {
            group->raiseSignalForGroup(siginfo);
        }
    }
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
        TerminalDisplay::printCharacter(buf[i]);
    }
    TerminalDisplay::updateCursorPosition();

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
    bool continuationByte;
    do {
        continuationByte = (circularBuffer[writeIndex - 1] & 0xC0) == 0x80;
        if (likely(writeIndex != 0)) {
            writeIndex = (writeIndex - 1) % TERMINAL_BUFFER_SIZE;
        } else {
            writeIndex = TERMINAL_BUFFER_SIZE - 1;
        }
    } while (continuationByte && lineIndex != writeIndex);

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
