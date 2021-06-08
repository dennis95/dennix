/* Copyright (c) 2016, 2017, 2018, 2019, 2020, 2021 Dennis Wölfing
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

/* kernel/include/dennix/kernel/terminal.h
 * Terminal class.
 */

#ifndef KERNEL_TERMINAL_H
#define KERNEL_TERMINAL_H

#include <dennix/termios.h>
#include <dennix/winsize.h>
#include <dennix/kernel/keyboard.h>
#include <dennix/kernel/vnode.h>

#define TERMINAL_BUFFER_SIZE 4096

class Terminal : public Vnode {
public:
    Terminal(dev_t dev);
    int devctl(int command, void* restrict data, size_t size,
            int* restrict info) override;
    int isatty() override;
    short poll() override;
    ssize_t read(void* buffer, size_t size, int flags) override;
    void setWinsize(struct winsize* ws);
    int tcgetattr(struct termios* result) override;
    int tcsetattr(int flags, const struct termios* termios) override;
    ssize_t write(const void* buffer, size_t size, int flags) override;
protected:
    void endLine();
    void handleCharacter(char c);
    virtual void output(const char* buffer, size_t size) = 0;
    void writeBuffer(char c);
private:
    size_t dataAvailable();
    bool backspace();
    bool hasIncompleteLine();
    void raiseSignal(int signal);
    char readBuffer();
    void resetBuffer();
protected:
    struct termios termio;
private:
    pid_t foregroundGroup;
    unsigned int numEof;
    kthread_cond_t readCond;
    kthread_cond_t writeCond;
    char circularBuffer[TERMINAL_BUFFER_SIZE];
    size_t readIndex;
    size_t lineIndex;
    size_t writeIndex;
    struct winsize winsize;
};

#endif
